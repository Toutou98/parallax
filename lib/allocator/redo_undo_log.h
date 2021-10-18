#pragma once
#include "../btree/btree.h"
#include "device_structures.h"
#include <aio.h>
#include <pthread.h>
#include <stdint.h>
#include <uthash.h>

#define RUL_LOG_CHUNK_NUM 8
#define RUL_SEGMENT_FOOTER_SIZE_IN_BYTES (RUL_LOG_CHUNK_NUM * ALIGNMENT_SIZE)
#define RUL_LOG_CHUNK_SIZE_IN_BYTES ((SEGMENT_SIZE - RUL_SEGMENT_FOOTER_SIZE_IN_BYTES) / RUL_LOG_CHUNK_NUM)
#define RUL_LOG_CHUNK_MAX_ENTRIES (RUL_LOG_CHUNK_SIZE_IN_BYTES / sizeof(struct rul_log_entry))
#define RUL_SEGMENT_MAX_ENTRIES ((RUL_LOG_CHUNK_NUM * RUL_LOG_CHUNK_MAX_ENTRIES) - 1)

enum rul_op_type { RUL_ALLOCATE = 1, RUL_FREE, RUL_COMMIT, RUL_LOG_ALLOCATE, RUL_LOG_FREE };
#define RUL_LOG_SYSTEM_TXN 0

struct rul_log_entry {
	uint64_t txn_id;
	uint64_t dev_offt;
	enum rul_op_type op_type;
	uint32_t size;
} __attribute__((packed, aligned(32)));

struct rul_log_segment {
	struct rul_log_entry chunk[RUL_LOG_CHUNK_NUM][RUL_LOG_CHUNK_MAX_ENTRIES];
	char pad[RUL_SEGMENT_FOOTER_SIZE_IN_BYTES - sizeof(uint64_t)];
	uint64_t next_seg_offt;
} __attribute__((packed));

#define RUL_ENTRIES_PER_TXN_BUFFER 512
struct rul_transaction {
	uint64_t txn_id;
	struct rul_transaction_buffer *head;
	struct rul_transaction_buffer *tail;
	UT_hash_handle hh;
};

struct rul_transaction_buffer {
	struct rul_log_entry txn_entry[RUL_ENTRIES_PER_TXN_BUFFER];
	struct rul_transaction_buffer *next;
	uint32_t n_entries;
};

struct rul_log_descriptor {
	struct rul_log_segment my_segment;
	pthread_mutex_t rul_lock;
	struct aiocb aiocbp[RUL_LOG_CHUNK_NUM];
	uint32_t pending_IO[RUL_LOG_CHUNK_NUM];
	struct rul_transaction *trans_map;
	uint32_t curr_chunk_id;
	uint32_t curr_chunk_entry;
	uint32_t curr_segment_entry;
	//recoverable staff
	uint64_t size;
	uint64_t txn_id;
	uint64_t tail_dev_offt;
	uint64_t head_dev_offt;
};

void rul_log_init(struct db_descriptor *db_desc);
uint64_t rul_start_txn(struct db_descriptor *db_desc);
int rul_add_entry_in_txn_buf(struct db_descriptor *db_desc, struct rul_log_entry *entry);
int rul_flush_txn(struct db_descriptor *db_desc, uint64_t txn_id);
