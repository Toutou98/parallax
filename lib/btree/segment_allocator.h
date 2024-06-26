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

#ifndef SEGMENT_ALLOCATOR_H
#define SEGMENT_ALLOCATOR_H
#include "../allocator/log_structures.h"
#include "btree.h"
#include "index_node.h"
#include <stdint.h>
struct node_header;
// IWYU pragma: no_forward_declare index_node

/*functions for index nodes*/
struct index_node *seg_get_index_node(struct db_descriptor *db_desc, uint8_t level_id, uint8_t tree_id, char reason);

struct index_node *seg_get_index_node_header(struct db_descriptor *db_desc, uint8_t level_id, uint8_t tree_id);

void seg_free_index_node_header(struct db_descriptor *db_desc, uint8_t level_id, uint8_t tree_id,
				struct node_header *node);

void seg_free_index_node(struct db_descriptor *db_desc, uint8_t level_id, uint8_t tree_id, struct index_node *inode);

/*function for leaf nodes*/
struct leaf_node *seg_get_leaf_node(struct db_descriptor *db_desc, uint8_t level_id, uint8_t tree_id);

struct leaf_node *seg_get_leaf_node_header(struct db_descriptor *db_desc, uint8_t level_id, uint8_t tree_id);

struct leaf_node *seg_get_dynamic_leaf_node(struct db_descriptor *db_desc, uint8_t level_id, uint8_t tree_id);
/*log related*/
segment_header *seg_get_raw_log_segment(struct db_descriptor *db_desc, enum log_type log_type, uint64_t txn_id);

void seg_free_segment(struct db_descriptor *db_desc, uint64_t txn_id, uint64_t seg_offt);

// struct segment_header *get_segment_for_lsm_level_IO(struct db_descriptor *db_desc, uint8_t level_id, uint8_t tree_id);

uint64_t seg_free_L0(struct db_descriptor *db_desc, uint8_t tree_id);
#endif
