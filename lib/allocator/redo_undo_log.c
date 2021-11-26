// Copyright [2021] [FORTH-ICS]
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "redo_undo_log.h"
#include "../btree/btree.h"
#include "../btree/conf.h"
#include "device_structures.h"
#include "volume_manager.h"
#include <aio.h>
#include <assert.h>
#include <errno.h>
#include <log.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <uthash.h>

static void rul_flush_log_chunk(struct db_descriptor *db_desc, uint32_t chunk_id)
{
	struct rul_log_descriptor *log_desc = db_desc->allocation_log;

	//check if pending
	while (log_desc->pending_IO[chunk_id]) {
		int state = aio_error(&log_desc->aiocbp[chunk_id]);
		switch (state) {
		case 0:
			log_desc->pending_IO[chunk_id] = 0;
			break;
		case EINPROGRESS:
			break;
		case ECANCELED:
			log_warn("Request cacelled");
			break;
		default:
			log_fatal("error appending to redo undo log");
			break;
		}
		ssize_t size = RUL_LOG_CHUNK_SIZE_IN_BYTES;
		ssize_t dev_offt = log_desc->tail_dev_offt + (chunk_id * RUL_LOG_CHUNK_SIZE_IN_BYTES);
		ssize_t bytes_written = 0;
		ssize_t total_bytes_written = 0;

		while (total_bytes_written < size) {
			bytes_written = pwrite(db_desc->db_volume->vol_fd,
					       db_desc->allocation_log->my_segment.chunk[chunk_id],
					       size - total_bytes_written, dev_offt + total_bytes_written);
			if (bytes_written == -1) {
				log_fatal("Failed to write DB's %s superblock", db_desc->db_superblock->db_name);
				perror("Reason");
				exit(EXIT_FAILURE);
			}
			total_bytes_written += bytes_written;
		}
	}
}

static void rul_aflush_log_chunk(struct db_descriptor *db_desc, uint32_t chunk_id)
{
	struct rul_log_descriptor *log_desc = db_desc->allocation_log;

	//check if pending
	while (log_desc->pending_IO[chunk_id]) {
		int state = aio_error(&log_desc->aiocbp[chunk_id]);
		switch (state) {
		case 0:
			log_desc->pending_IO[chunk_id] = 0;
			break;
		case EINPROGRESS:
			break;
		case ECANCELED:
			log_warn("Request cacelled");
			break;
		default:
			log_fatal("error appending to redo undo log");
			break;
		}
	}

	//Prepare an async IO request
	memset(&log_desc->aiocbp[chunk_id], 0x00, sizeof(struct aiocb));
	log_desc->aiocbp[chunk_id].aio_fildes = db_desc->db_volume->vol_fd;
	log_desc->aiocbp[chunk_id].aio_offset = log_desc->tail_dev_offt + (chunk_id * RUL_LOG_CHUNK_SIZE_IN_BYTES);
	assert(log_desc->aiocbp[chunk_id].aio_offset % ALIGNMENT_SIZE == 0);
	log_desc->aiocbp[chunk_id].aio_buf = log_desc->my_segment.chunk[chunk_id];
	assert((uint64_t)log_desc->aiocbp[chunk_id].aio_buf % ALIGNMENT_SIZE == 0);
	log_desc->aiocbp[chunk_id].aio_nbytes = RUL_LOG_CHUNK_SIZE_IN_BYTES;
	log_desc->pending_IO[chunk_id] = 1;

	//Now issue the async IO
	if (aio_write(&log_desc->aiocbp[chunk_id])) {
		log_fatal("IO failed for redo undo log");
		exit(EXIT_FAILURE);
	}

	uint32_t i = chunk_id;
	while (log_desc->pending_IO[i]) {
		int state = aio_error(&log_desc->aiocbp[i]);
		switch (state) {
		case 0:
			log_desc->pending_IO[i] = 0;
			break;
		case EINPROGRESS:
			break;
		case ECANCELED:
			log_warn("Request cacelled");
			break;
		default:
			log_fatal("error appending to redo undo log for chunk %u  state is %d Reason is %s", i, state,
				  strerror(state));
			assert(0);
			exit(EXIT_FAILURE);
		}
	}
}

static void rul_wait_all_chunk_IOs(struct db_descriptor *db_desc, uint32_t chunk_num)
{
	struct rul_log_descriptor *log_desc = db_desc->allocation_log;
	for (uint32_t i = 0; i < chunk_num; ++i) {
		//check if pending
		while (log_desc->pending_IO[i]) {
			int state = aio_error(&log_desc->aiocbp[i]);
			switch (state) {
			case 0:
				log_desc->pending_IO[i] = 0;
				break;
			case EINPROGRESS:
				break;
			case ECANCELED:
				log_warn("Request cacelled");
				break;
			default:
				log_fatal("error appending to redo undo log for chunk %u  state is %d Reason is %s", i,
					  state, strerror(state));
				assert(0);
				exit(EXIT_FAILURE);
			}
		}
	}
}

static void rul_flush_last_chunk(struct db_descriptor *db_desc)
{
	struct rul_log_descriptor *log_desc = db_desc->allocation_log;
	// Write with explicit I/O the segment_header
	ssize_t total_bytes_written = 0;
	ssize_t bytes_written = 0;
	ssize_t size;
	ssize_t dev_offt;
	uint32_t chunk_id = RUL_LOG_CHUNK_NUM - 1;

	rul_wait_all_chunk_IOs(db_desc, RUL_LOG_CHUNK_NUM - 1);

	for (uint32_t i = 0; i < RUL_LOG_CHUNK_NUM; ++i) {
		//check if pending
		while (log_desc->pending_IO[i]) {
			int state = aio_error(&log_desc->aiocbp[i]);
			switch (state) {
			case 0:
				log_desc->pending_IO[i] = 0;
				break;
			case EINPROGRESS:
				break;
			case ECANCELED:
				log_warn("Request cacelled");
				break;
			default:
				log_fatal("error appending to redo undo log");
				break;
			}
		}
	}
	size = RUL_LOG_CHUNK_SIZE_IN_BYTES + RUL_SEGMENT_FOOTER_SIZE_IN_BYTES;
	dev_offt = (db_desc->allocation_log->head_dev_offt + SEGMENT_SIZE) - size;

	while (total_bytes_written < size) {
		bytes_written = pwrite(db_desc->db_volume->vol_fd, db_desc->allocation_log->my_segment.chunk[chunk_id],
				       size - total_bytes_written, dev_offt + total_bytes_written);
		if (bytes_written == -1) {
			log_fatal("Failed to write DB's %s superblock", db_desc->db_superblock->db_name);
			perror("Reason");
			exit(EXIT_FAILURE);
		}
		total_bytes_written += bytes_written;
	}
}

static void rul_read_last_segment(struct db_descriptor *db_desc)
{
	(void)db_desc;
	log_fatal("Fix it gesalous");
	exit(EXIT_FAILURE);
#if 0
	ssize_t total_bytes_read = 0;
	ssize_t bytes_read = 0;
	ssize_t size = SEGMENT_SIZE;
	while (total_bytes_read < size) {
		bytes_read = pwrite(db_desc->my_volume->vol_fd, &db_desc->allocation_log->my_segment,
				    size - total_bytes_read, db_desc->my_superblock_offt + total_bytes_read);
		if (bytes_read == -1) {
			log_fatal("Failed to write DB's %s superblock", db_desc->db_name);
			perror("Reason");
			exit(EXIT_FAILURE);
		}
		total_bytes_read += bytes_read;
	}
#endif
}

/**
 * Appends a new entry in the redo-undo log
 *
 */
static int rul_append(struct db_descriptor *db_desc, struct rul_log_entry *entry)
{
	int ret = 0;
	struct rul_log_descriptor *log_desc = db_desc->allocation_log;

	if (log_desc->curr_segment_entry > RUL_SEGMENT_MAX_ENTRIES) {
		// Time to add a new segment
		uint64_t new_tail_dev_offt = (uint64_t)mem_allocate(db_desc->db_volume, SEGMENT_SIZE);
		log_desc->my_segment.next_seg_offt = new_tail_dev_offt;
		struct rul_log_entry e;
		e.txn_id = 0;
		e.dev_offt = new_tail_dev_offt;
		e.op_type = RUL_LOG_ALLOCATE;
		//add new entry in the memory segment
		memcpy(&log_desc->my_segment.chunk[log_desc->curr_chunk_id][log_desc->curr_chunk_entry], &e,
		       sizeof(struct rul_log_entry));
		log_desc->size += (sizeof(struct rul_log_entry) + RUL_SEGMENT_FOOTER_SIZE_IN_BYTES);
		log_desc->my_segment.next_seg_offt = new_tail_dev_offt;
		log_desc->tail_dev_offt = new_tail_dev_offt;
		log_desc->curr_chunk_id = 0;
		log_desc->curr_chunk_entry = 0;
		log_desc->curr_segment_entry = 0;
		rul_flush_last_chunk(db_desc);
#if 0
		//update and write superblock
		db_desc->my_superblock.allocation_log.tail_dev_offt = log_desc->tail_dev_offt;
		db_desc->my_superblock.allocation_log.size = log_desc->size;
		db_desc->my_superblock.allocation_log.txn_id = log_desc->txn_id;
		pr_lock_db_superblock(db_desc);
		pr_flush_db_superblock(db_desc);
		pr_unlock_db_superblock(db_desc);
#endif
		memset(&log_desc->my_segment, 0x00, sizeof(struct rul_log_segment));
	}

	if (log_desc->curr_chunk_entry >= RUL_LOG_CHUNK_MAX_ENTRIES) {
		rul_aflush_log_chunk(db_desc, log_desc->curr_chunk_id);
		++log_desc->curr_chunk_id;
		log_desc->curr_chunk_entry = 0;
	}

	//log_info("Segment entry: %u Chunk id %u chunk entry %u", log_desc->curr_segment_entry, log_desc->curr_chunk_id,
	//	 log_desc->curr_chunk_entry);
	//Finally append
	memcpy(&log_desc->my_segment.chunk[log_desc->curr_chunk_id][log_desc->curr_chunk_entry], entry,
	       sizeof(struct rul_log_entry));
	log_desc->size += sizeof(struct rul_log_entry);
	++log_desc->curr_chunk_entry;
	++log_desc->curr_segment_entry;
	return ret;
}

void rul_log_destroy(struct db_descriptor *db_desc)
{
	MUTEX_LOCK(&db_desc->allocation_log->rul_lock);
	free(db_desc->allocation_log);
	MUTEX_UNLOCK(&db_desc->allocation_log->rul_lock);
}

void rul_log_init(struct db_descriptor *db_desc)
{
	_Static_assert(RUL_LOG_CHUNK_NUM % 2 == 0, "RUL_LOG_CHUNK_NUM invalid!");
	_Static_assert(RUL_LOG_CHUNK_SIZE_IN_BYTES % ALIGNMENT_SIZE == 0, "RUL_LOG_CHUNK_SIZE_IN_BYTES not aligned!");
	_Static_assert(sizeof(struct rul_log_entry) + sizeof(uint64_t) <= ALIGNMENT_SIZE,
		       "Redo-undo log footer must be <= 512");
	_Static_assert(sizeof(struct rul_log_segment) == SEGMENT_SIZE,
		       "Redo undo log segment not equal to SEGMENT_SIZE!");

	struct rul_log_descriptor *log_desc;
	if (posix_memalign((void **)&log_desc, ALIGNMENT, sizeof(struct rul_log_descriptor)) != 0) {
		log_fatal("Failed to allocate redo_undo_log descriptor buffer");
		exit(EXIT_FAILURE);
	}
	memset(log_desc, 0x00, sizeof(struct rul_log_descriptor));

	pr_lock_db_superblock(db_desc);

	MUTEX_INIT(&log_desc->rul_lock, NULL);
	MUTEX_INIT(&log_desc->trans_map_lock, NULL);
	// resume state, superblock must have been read in memory
	log_desc->head_dev_offt = db_desc->db_superblock->allocation_log.head_dev_offt;
	log_desc->tail_dev_offt = db_desc->db_superblock->allocation_log.tail_dev_offt;
	log_desc->size = db_desc->db_superblock->allocation_log.size;
	log_desc->trans_map = NULL;
	log_desc->txn_id = db_desc->db_superblock->allocation_log.txn_id;

	db_desc->allocation_log = log_desc;

	if (log_desc->head_dev_offt == 0) {
		// empty log do the first allocation
		uint64_t head_dev_offt = (uint64_t)mem_allocate(db_desc->db_volume, SEGMENT_SIZE);
		if (!head_dev_offt) {
			log_fatal("Out of Space!");
			assert(0);
			exit(EXIT_FAILURE);
		}
		log_desc->head_dev_offt = head_dev_offt;
		log_desc->tail_dev_offt = head_dev_offt;
		log_desc->txn_id = 1;
		log_desc->size = 0;

		struct rul_log_entry e;
		e.txn_id = 0;
		e.dev_offt = head_dev_offt;
		e.op_type = RUL_LOG_ALLOCATE;
		MUTEX_LOCK(&log_desc->rul_lock);
		rul_append(db_desc, &e);
		MUTEX_UNLOCK(&log_desc->rul_lock);
		// Flush DB's superblock
		pr_flush_db_superblock(db_desc);
	} else
		// Read last segment in memory
		rul_read_last_segment(db_desc);

	pr_unlock_db_superblock(db_desc);

	uint32_t tail_size = log_desc->size % SEGMENT_SIZE;
	uint32_t n_entries_in_tail = tail_size / sizeof(struct rul_log_entry);
	log_desc->curr_segment_entry = n_entries_in_tail;
	log_desc->curr_chunk_id = n_entries_in_tail / RUL_LOG_CHUNK_MAX_ENTRIES;
	log_desc->curr_chunk_entry = n_entries_in_tail % RUL_LOG_CHUNK_MAX_ENTRIES;
}

uint64_t rul_start_txn(struct db_descriptor *db_desc)
{
	struct rul_log_descriptor *log_desc = db_desc->allocation_log;
	uint64_t txn_id = __sync_fetch_and_add(&log_desc->txn_id, 1);
	//log_info("Start transaction %llu id curr segment entry %llu", txn_id, db_desc->log_desc->curr_segment_entry);
	// check if (accidentally) txn exists already
	struct rul_transaction *my_transaction;

	MUTEX_LOCK(&log_desc->trans_map_lock);
	HASH_FIND_PTR(log_desc->trans_map, &txn_id, my_transaction);
	if (my_transaction != NULL) {
		log_fatal("Txn %llu already exists (it shouldn't)", txn_id);
		exit(EXIT_FAILURE);
	}
	my_transaction = calloc(1, sizeof(struct rul_transaction));
	my_transaction->txn_id = txn_id;
	struct rul_transaction_buffer *my_trans_buf = calloc(1, sizeof(struct rul_transaction_buffer));
	my_trans_buf->next = NULL;
	my_trans_buf->n_entries = 0;
	my_transaction->head = my_trans_buf;
	my_transaction->tail = my_trans_buf;

	HASH_ADD_PTR(log_desc->trans_map, txn_id, my_transaction);
	MUTEX_UNLOCK(&log_desc->trans_map_lock);
	return txn_id;
}

int rul_add_entry_in_txn_buf(struct db_descriptor *db_desc, struct rul_log_entry *entry)
{
	struct rul_log_descriptor *log_desc = db_desc->allocation_log;
	uint64_t txn_id = entry->txn_id;
	struct rul_transaction *my_transaction;

	MUTEX_LOCK(&log_desc->trans_map_lock);
	HASH_FIND_PTR(log_desc->trans_map, &txn_id, my_transaction);

	if (my_transaction == NULL) {
		log_fatal("Txn %llu not found!", txn_id);
		assert(0);
		exit(EXIT_FAILURE);
	}
	MUTEX_UNLOCK(&log_desc->trans_map_lock);

	struct rul_transaction_buffer *my_trans_buf = my_transaction->tail;
	// Is there enough space
	if (my_trans_buf->n_entries >= RUL_ENTRIES_PER_TXN_BUFFER) {
		struct rul_transaction_buffer *new_trans_buf = calloc(1, sizeof(struct rul_transaction_buffer));
		my_trans_buf->next = new_trans_buf;
		new_trans_buf->next = NULL;
		new_trans_buf->n_entries = 0;
		my_transaction->tail = new_trans_buf;

		my_trans_buf = new_trans_buf;
	}

	my_trans_buf->txn_entry[my_trans_buf->n_entries] = *entry;
	++my_trans_buf->n_entries;
	return 1;
}

struct rul_log_info rul_flush_txn(struct db_descriptor *db_desc, uint64_t txn_id)
{
	struct rul_log_info info = { .head_dev_offt = 0, .tail_dev_offt = 0, .size = 0, .txn_id = 0 };
	struct rul_log_descriptor *log_desc = db_desc->allocation_log;
	struct rul_transaction *my_transaction;

	HASH_FIND_PTR(log_desc->trans_map, &txn_id, my_transaction);

	if (my_transaction == NULL) {
		log_fatal("Txn %llu not found!", txn_id);
		assert(0);
		exit(EXIT_FAILURE);
	}

	MUTEX_LOCK(&log_desc->rul_lock);
	struct rul_transaction_buffer *curr = my_transaction->head;
	assert(curr != NULL);
	while (curr) {
		for (uint32_t i = 0; i < curr->n_entries; ++i) {
			rul_append(db_desc, &curr->txn_entry[i]);
		}
		curr = curr->next;
	}

	if (log_desc->curr_chunk_id > 0)
		rul_wait_all_chunk_IOs(db_desc, log_desc->curr_chunk_id - 1);

	//synchronously write last
	rul_flush_log_chunk(db_desc, log_desc->curr_chunk_id);
	info.head_dev_offt = db_desc->allocation_log->head_dev_offt;
	info.tail_dev_offt = db_desc->allocation_log->tail_dev_offt;
	info.size = db_desc->allocation_log->size;
	info.txn_id = db_desc->allocation_log->txn_id;

	MUTEX_UNLOCK(&log_desc->rul_lock);
	return info;
}

void rul_apply_txn_buf_freeops_and_destroy(struct db_descriptor *db_desc, uint64_t txn_id)
{
	struct rul_log_descriptor *log_desc = db_desc->allocation_log;
	struct rul_transaction *my_transaction;

	MUTEX_LOCK(&log_desc->trans_map_lock);
	HASH_FIND_PTR(log_desc->trans_map, &txn_id, my_transaction);

	if (my_transaction == NULL) {
		log_fatal("Txn %llu not found!", txn_id);
		assert(0);
		exit(EXIT_FAILURE);
	}
	MUTEX_UNLOCK(&log_desc->trans_map_lock);

	struct rul_transaction_buffer *curr = my_transaction->head;
	assert(curr != NULL);
	while (curr) {
		for (uint32_t i = 0; i < curr->n_entries; ++i) {
			rul_append(db_desc, &curr->txn_entry[i]);
			switch (curr->txn_entry[i].op_type) {
			case RUL_FREE:
				mem_bitmap_mark_block_free(db_desc->db_volume, curr->txn_entry[i].dev_offt);
				break;
			case RUL_ALLOCATE:
				break;
			default:
				log_fatal("Unhandled case probably corruption in txn buffer");
				assert(0);
				exit(EXIT_FAILURE);
			}
		}
		struct rul_transaction_buffer *del = curr;
		curr = curr->next;
		free(del);
		del = NULL;
	}

	MUTEX_LOCK(&log_desc->trans_map_lock);
	HASH_DEL(log_desc->trans_map, my_transaction);
	MUTEX_UNLOCK(&log_desc->trans_map_lock);
	free(my_transaction);
}
