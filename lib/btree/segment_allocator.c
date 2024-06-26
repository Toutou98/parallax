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
#include "segment_allocator.h"
#include "../allocator/device_structures.h"
#include "../allocator/log_structures.h"
#include "../allocator/region_log.h"
#include "../allocator/volume_manager.h"
#include "../common/common.h"
#include "btree_node.h"
#include "conf.h"
#include "index_node.h"
#include <assert.h>
#include <log.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
// IWYU pragma: no_forward_declare index_node
// IWYU pragma: no_forward_declare leaf_node

struct link_segments_metadata {
	struct L0_descriptor *level_desc;
	segment_header *new_segment;
	uint64_t segment_id;
	uint64_t available_space;
	uint64_t tree_id;
	int in_mem;
};

// cppcheck-suppress unusedFunction
void seg_free_segment(struct db_descriptor *db_desc, uint64_t txn_id, uint64_t seg_offt)
{
	struct regl_log_entry log_entry = {
		.dev_offt = seg_offt, .txn_id = txn_id, .op_type = REGL_FREE, .size = SEGMENT_SIZE
	};

	regl_add_entry_in_txn_buf(db_desc, &log_entry);
}

static uint64_t link_memory_segments(struct link_segments_metadata *req)
{
	struct L0_descriptor *level_desc = req->level_desc;
	segment_header *new_segment = req->new_segment;
	uint64_t available_space = req->available_space;
	uint64_t segment_id = req->segment_id;
	uint8_t tree_id = req->tree_id;

	if (req->level_desc->offset[req->tree_id] != 0) {
		/*chain segments*/
		new_segment->next_segment = NULL;
		new_segment->prev_segment = (segment_header *)ABSOLUTE_ADDRESS(level_desc->last_segment[tree_id]);
		level_desc->last_segment[tree_id]->next_segment = (segment_header *)ABSOLUTE_ADDRESS(new_segment);
		level_desc->last_segment[tree_id] = new_segment;
		level_desc->last_segment[tree_id]->segment_id = segment_id + 1;
		level_desc->offset[tree_id] += (available_space + sizeof(segment_header));
	} else {
		/*special case for the first segment for this level*/
		new_segment->next_segment = NULL;
		new_segment->prev_segment = NULL;
		level_desc->first_segment[tree_id] = new_segment;
		level_desc->last_segment[tree_id] = new_segment;
		level_desc->last_segment[tree_id]->segment_id = 1;
		level_desc->offset[tree_id] = sizeof(segment_header);
	}

	return level_desc->offset[tree_id] % SEGMENT_SIZE;
}

static void set_link_segments_metadata(struct link_segments_metadata *req, segment_header *new_segment,
				       uint64_t segment_id, uint64_t available_space)
{
	req->new_segment = new_segment;
	req->segment_id = segment_id;
	req->available_space = available_space;
}

static void *get_space(struct db_descriptor *db_desc, uint8_t level_id, uint8_t tree_id, uint32_t size)
{
	if (0 != level_id) {
		log_fatal("Allocations only for Level-0");
		_exit(EXIT_FAILURE);
	}

	struct L0_descriptor *level_desc = &db_desc->L0;

	struct link_segments_metadata req = { .level_desc = level_desc, .tree_id = tree_id };
	segment_header *new_segment = NULL;
	struct node_header *node = NULL;
	uint32_t available_space;
	uint64_t offset_in_segment = 0;
	uint64_t segment_id;

	MUTEX_LOCK(&level_desc->level_allocation_lock);

	/*check if we have enough space to satisfy the request*/
	if (level_desc->offset[tree_id] % SEGMENT_SIZE == 0) {
		available_space = 0;
		segment_id = 0;
	} else {
		available_space = SEGMENT_SIZE - (level_desc->offset[tree_id] % SEGMENT_SIZE);
		offset_in_segment = level_desc->offset[tree_id] % SEGMENT_SIZE;
		segment_id = level_desc->last_segment[tree_id]->segment_id;
	}
	if (available_space < size) {
		//Characterize remaining empty space if any as paddedSpace
		if (available_space > 0) {
			int *pad = (int *)((uint64_t)level_desc->last_segment[tree_id] +
					   (level_desc->offset[tree_id] % SEGMENT_SIZE));
			*pad = paddedSpace;
		}

		if (posix_memalign((void **)&new_segment, ALIGNMENT_SIZE, SEGMENT_SIZE) != 0) {
			log_fatal("MEMALIGN FAILED");
			BUG_ON();
		}
		req.in_mem = 1;

		assert(new_segment);
		set_link_segments_metadata(&req, new_segment, segment_id, available_space);
		offset_in_segment = link_memory_segments(&req);
	}

	node = (struct node_header *)((uintptr_t)level_desc->last_segment[tree_id] + offset_in_segment);
	assert(node);
	level_desc->offset[tree_id] += size;
	MUTEX_UNLOCK(&level_desc->level_allocation_lock);
	return node;
}

struct index_node *seg_get_index_node(struct db_descriptor *db_desc, uint8_t level_id, uint8_t tree_id, char reason)
{
	(void)reason;
	return get_space(db_desc, level_id, tree_id, index_node_get_size());
}

void seg_free_index_node(struct db_descriptor *db_desc, uint8_t level_id, uint8_t tree_id, struct index_node *inode)
{
	//leave for future use
	(void)db_desc;
	(void)level_id;
	(void)tree_id;
	(void)inode;
}

struct leaf_node *seg_get_leaf_node(struct db_descriptor *db_desc, uint8_t level_id, uint8_t tree_id)
{
	const struct L0_descriptor *level0 = &db_desc->L0;

	struct leaf_node *leaf = (struct leaf_node *)get_space(db_desc, level_id, tree_id, level0->leaf_size);
	return leaf;
}

struct leaf_node *seg_get_dynamic_leaf_node(struct db_descriptor *db_desc, uint8_t level_id, uint8_t tree_id)
{
	const struct L0_descriptor *level0 = &db_desc->L0;
	return get_space(db_desc, level_id, tree_id, level0->leaf_size);
}

segment_header *seg_get_raw_log_segment(struct db_descriptor *db_desc, enum log_type log_type, uint64_t txn_id)
{
	enum regl_op_type op_type;
	switch (log_type) {
	case BIG_LOG:
		op_type = REGL_LARGE_LOG_ALLOCATE;
		break;
	case MEDIUM_LOG:
		op_type = REGL_MEDIUM_LOG_ALLOCATE;
		break;
	case SMALL_LOG:
		op_type = REGL_SMALL_LOG_ALLOCATE;
		break;
	default:
		log_fatal("Unknown log type");
		BUG_ON();
	}

	struct regl_log_entry log_entry = { .dev_offt = mem_allocate(db_desc->db_volume, SEGMENT_SIZE),
					    .txn_id = txn_id,
					    .op_type = op_type,
					    .size = SEGMENT_SIZE };
	regl_add_entry_in_txn_buf(db_desc, &log_entry);
	segment_header *segment = (segment_header *)REAL_ADDRESS(log_entry.dev_offt);
	return segment;
}

uint64_t seg_free_L0(struct db_descriptor *db_desc, uint8_t tree_id)
{
	struct segment_header *curr_segment = db_desc->L0.first_segment[tree_id];
	if (!curr_segment) {
		log_debug("Level [%u][%u] is free nothing to do", 0, tree_id);
		return 0;
	}

	uint64_t space_freed = 0;

	while (curr_segment) {
		struct segment_header *stale_seg = curr_segment;
		curr_segment = stale_seg->next_segment == NULL ? NULL : REAL_ADDRESS(stale_seg->next_segment);
		free(stale_seg);
		space_freed += SEGMENT_SIZE;
	}

	log_debug("Freed in-memory L0 level [%u][%u] for db %s", 0, tree_id, db_desc->db_superblock->db_name);
	assert(space_freed == db_desc->L0.offset[tree_id] % SEGMENT_SIZE ?
		       db_desc->L0.offset[tree_id] :
		       db_desc->L0.offset[tree_id] / SEGMENT_SIZE + SEGMENT_SIZE);
	return space_freed;
}
