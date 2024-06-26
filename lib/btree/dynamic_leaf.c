// Copyright [1] [FORTH-ICS]
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

#include "dynamic_leaf.h"
#include "../btree/kv_pairs.h"
#include "../common/common.h"
#include "btree_node.h"
#include "parallax/structures.h"
#include <assert.h>
#include <log.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if 0
static bool dl_check_leaf(struct leaf_node *leaf);
#endif

struct dl_slot_array {
	// The index points to the location of the kv pair in the leaf.
	// uint16_t key_category : 2;
	// Tombstone notifies if the key is deleted.
	// uint16_t tombstone : 1;
	uint16_t index;
};

struct leaf_node {
	struct node_header header;
} __attribute__((packed));

struct dl_slot_array *dl_get_slot_array_offset(const struct leaf_node *leaf)
{
	return (struct dl_slot_array *)(((char *)leaf) + sizeof(struct leaf_node));
}

struct kv_splice_base dl_get_general_splice(struct leaf_node *leaf, int32_t position)
{
	struct kv_splice_base general_splice = { 0 };
	struct dl_slot_array *slot_array = dl_get_slot_array_offset(leaf);
	// general_splice.kv_cat = slot_array[position].key_category;
	// general_splice.is_tombstone = slot_array[position].tombstone;
	char *leaf_buf = (char *)leaf;
	general_splice.kv_cat = kv_meta_get_cat((struct kv_splice_meta *)&leaf_buf[slot_array[position].index]);
	general_splice.is_tombstone =
		kv_meta_is_tombstone((struct kv_splice_meta *)&leaf_buf[slot_array[position].index]);

	uint8_t *kv_addr = (uint8_t *)leaf + slot_array[position].index;
	if (general_splice.kv_cat == SMALL_INPLACE || general_splice.kv_cat == MEDIUM_INPLACE) {
		general_splice.kv_splice = (struct kv_splice *)kv_addr;
		general_splice.kv_type = KV_FORMAT;
	} else if (general_splice.kv_cat == MEDIUM_INLOG || general_splice.kv_cat == BIG_INLOG) {
		general_splice.kv_sep2 = (struct kv_seperation_splice2 *)kv_addr;
		general_splice.kv_type = KV_PREFIX;
	} else {
		log_fatal("Unknown kv category");
		BUG_ON();
	}
	return general_splice;
}

static void dl_fill_key_from_general_splice(struct kv_splice_base *general_splice, char **key, int32_t *key_size)
{
	if (general_splice->kv_cat == SMALL_INPLACE || general_splice->kv_cat == MEDIUM_INPLACE) {
		*key = kv_splice_get_key_offset_in_kv(general_splice->kv_splice);
		*key_size = kv_splice_get_key_size(general_splice->kv_splice);
	} else if (general_splice->kv_cat == MEDIUM_INLOG || general_splice->kv_cat == BIG_INLOG) {
		*key = kv_sep2_get_key(general_splice->kv_sep2);
		*key_size = kv_sep2_get_key_size(general_splice->kv_sep2);
	} else {
		log_fatal("Unknown kv category");
		BUG_ON();
	}
}

int32_t dl_search_get_pos(struct leaf_node *leaf, const char *key, int32_t key_size, bool *exact_match)
{
	*exact_match = false;
	if (NULL == leaf || leaf->header.num_entries == 0)
		return -1;

	int32_t cmp_return_value = 0;
	int32_t start = 0;
	int32_t end = leaf->header.num_entries - 1;

	int32_t middle = 0;
	while (start <= end) {
		middle = (start + end) / 2;

		struct kv_splice_base leaf_splice = dl_get_general_splice(leaf, middle);

		/*At zero position we have a guard or -oo*/
		char *leaf_key = NULL;
		int32_t leaf_key_size = 0;
		dl_fill_key_from_general_splice(&leaf_splice, &leaf_key, &leaf_key_size);
		// log_debug(
		// 	"Comparing leaf key size: %d leaf key data %.*s  pos is %d with look up key size: %d key data %.*s",
		// 	leaf_key_size, leaf_key_size, leaf_key, middle, key_size, key_size, key);
		assert(leaf_key_size > 0);

		cmp_return_value = memcmp(leaf_key, key, key_size <= leaf_key_size ? key_size : leaf_key_size);

		if (0 == cmp_return_value && leaf_key_size == key_size) {
			*exact_match = true;
			// log_debug("Found at Leaf is %p num entries %d pos is %d", (void *)leaf,
			// 	  leaf->header.num_entries, middle);
			return middle;
		}

		if (0 == cmp_return_value) {
			// log_debug(
			// 	"Partial match leaf key size: %d leaf key %.*s | lookup key size %d lookup key %.*s middle %d leaf entries %d",
			// 	leaf_key_size, leaf_key_size, leaf_key, key_size, key_size, key, middle,
			// 	leaf->header.num_entries);
			cmp_return_value = leaf_key_size - key_size;
		}
		if (cmp_return_value > 0)
			end = middle - 1;
		else
			start = middle + 1;
	}

	return cmp_return_value > 0 ? middle - 1 : middle;
}

struct kv_splice_base dl_find_kv_in_dynamic_leaf(struct leaf_node *leaf, const char *key, int32_t key_size,
						 const char **error)
{
	struct kv_splice_base kv_not_found = { 0 };
	bool exact_match = false;
	int32_t pos = dl_search_get_pos(leaf, key, key_size, &exact_match);

	if (exact_match)
		return dl_get_general_splice(leaf, pos);
	*error = "KV pair not found";
	return kv_not_found;
}

bool dl_is_leaf_full(struct leaf_node *leaf, uint32_t kv_size)
{
	const uint8_t *left_border = (uint8_t *)leaf + sizeof(struct node_header) +
				     ((leaf->header.num_entries + 1) * sizeof(struct dl_slot_array));

	const uint8_t *right_border = (uint8_t *)leaf + leaf->header.log_size;
	right_border -= kv_size;

	return !(right_border > left_border);
}

static uint16_t dl_append_data_splice_in_dynamic_leaf(struct leaf_node *leaf, struct kv_splice_base *general_splice)
{
	int32_t kv_size = kv_splice_base_calculate_size(general_splice);
	if (dl_is_leaf_full(leaf, kv_size)) {
		log_warn("Leaf is full cannot serve request");
		assert(0);
		return 0;
	}
	assert(leaf->header.log_size > kv_size);
	leaf->header.log_size -= kv_size;
	char *src = (char *)leaf;
	char *dest = &src[leaf->header.log_size];
	kv_splice_base_serialize(general_splice, dest, kv_size);

	return leaf->header.log_size;
}

bool dl_append_splice_in_dynamic_leaf(struct leaf_node *leaf, struct kv_splice_base *general_splice, bool is_tombstone)
{
	uint16_t offt = dl_append_data_splice_in_dynamic_leaf(leaf, general_splice);
	if (!offt) {
		log_fatal("Leaf is full cannot serve request to avoid overflow (it shouldn't at this point)");
		_exit(EXIT_FAILURE);
	}
	struct dl_slot_array *slot_array = dl_get_slot_array_offset(leaf);
	char *leaf_buf = (char *)leaf;
	slot_array[leaf->header.num_entries++].index = offt;
	kv_meta_set_tombstone((struct kv_splice_meta *)&leaf_buf[offt], is_tombstone);
	kv_meta_set_cat((struct kv_splice_meta *)&leaf_buf[offt], general_splice->kv_cat);
	return true;
}

bool dl_insert_in_dynamic_leaf(struct leaf_node *leaf, struct kv_splice_base *splice, bool is_tombstone,
			       bool *exact_match)
{
	if (dl_is_leaf_full(leaf, kv_splice_base_calculate_size(splice))) {
		log_warn("Cannot serve request leaf will overflow");
		// assert(0);
		// _exit(EXIT_FAILURE);
		return false;
	}
	// if (!dl_check_leaf(leaf)) {
	// 	log_debug("Faulting splice is %d %s leaf entries = %d", kv_splice_base_get_key_size(splice),
	// 		  kv_splice_base_get_key_buf(splice), leaf->header.num_entries);
	// 	assert(0);
	// }
	int32_t kv_size = kv_splice_base_calculate_size(splice);
	if (dl_is_leaf_full(leaf, kv_size))
		return false;

	char *key = NULL;
	int32_t key_size = 0;

	dl_fill_key_from_general_splice(splice, &key, &key_size);

	int32_t pos = dl_search_get_pos(leaf, key, key_size, exact_match);
	struct dl_slot_array *slot_array = dl_get_slot_array_offset(leaf);
	if (*exact_match) {
		struct kv_splice_base updated_kv = dl_get_general_splice(leaf, pos);
		leaf->header.fragmentation += kv_splice_base_get_size(&updated_kv);
	} else {
		size_t bytes_to_move = (leaf->header.num_entries - (pos + 1)) * sizeof(struct dl_slot_array);
		// log_debug("Moving slot array from pos %d to pos %d gona move %lu bytes entris in leaf %d", pos + 1,
		// 	  pos + 2, bytes_to_move, leaf->header.num_entries);
		if (bytes_to_move)
			memmove(&slot_array[pos + 2], &slot_array[pos + 1], bytes_to_move);
	}
	uint16_t offt = dl_append_data_splice_in_dynamic_leaf(leaf, splice);
	if (!offt) {
		log_fatal("Leaf is full cannot fullfill the request (it shouldn't at this point)");
		_exit(EXIT_FAILURE);
	}

	if (!*exact_match) {
		pos = pos + 1;
		++leaf->header.num_entries;
	}
	char *leaf_buf = (char *)leaf;
	slot_array[pos].index = offt;
	kv_meta_set_tombstone((struct kv_splice_meta *)&leaf_buf[offt], is_tombstone);
	kv_meta_set_cat((struct kv_splice_meta *)&leaf_buf[offt], splice->kv_cat);
	// if (!dl_check_leaf(leaf)) {
	// 	log_debug(
	// 		"Faulting splice is %d %s leaf entries = %d key was %.*s pos is %d num entries %d offt was %u",
	// 		kv_splice_base_get_key_size(splice), kv_splice_base_get_key_buf(splice),
	// 		leaf->header.num_entries, key_size, key, pos, leaf->header.num_entries, offt);
	// 	assert(0);
	// }
	return true;
}

void dl_init_leaf_iterator(struct leaf_node *leaf, struct leaf_iterator *iter, const char *key, int32_t key_size)
{
	iter->leaf = leaf;
	if (iter->leaf->header.num_entries <= 0) {
		iter->pos = -1;
		return;
	}
	iter->pos = 0;
	if (NULL != key) {
		bool exact_match = false;
		iter->pos = dl_search_get_pos(leaf, key, key_size, &exact_match);
	}
	iter->splice = dl_get_general_splice(leaf, iter->pos);
}

bool dl_is_leaf_iterator_valid(struct leaf_iterator *iter)
{
	return iter->leaf->header.num_entries > 0 && iter->pos < iter->leaf->header.num_entries;
}

void dl_leaf_iterator_next(struct leaf_iterator *iter)
{
	++iter->pos;
}

struct kv_splice_base dl_leaf_iterator_curr(struct leaf_iterator *iter)
{
	if (!dl_is_leaf_iterator_valid(iter)) {
		struct kv_splice_base splice = { 0 };
		return splice;
	}

	return dl_get_general_splice(iter->leaf, iter->pos);
}

// static bool dl_check_leaf(struct leaf_node *leaf)
// {
// 	struct dl_leaf_iterator iter = { 0 };
// 	dl_init_leaf_iterator(leaf, &iter, NULL, -1);
// 	while (dl_is_leaf_iterator_valid(&iter)) {
// 		struct kv_splice_base splice = dl_leaf_iterator_curr(&iter);
// 		if (kv_splice_base_get_key_size(&splice) <= 0) {
// 			log_debug("Assertion failed iterator pos %d  leaf entries: %d cat = %d", iter.pos,
// 				  iter.leaf->header.num_entries, splice.cat);
// 			return false;
// 		}
// 		if (kv_splice_base_get_key_size(&splice) > MAX_KEY_SIZE) {
// 			log_debug("Assertion failed iterator pos %d", iter.pos);
// 			assert(0);
// 		}
// 		dl_leaf_iterator_next(&iter);
// 	}
// 	return true;
// }

struct kv_splice_base dl_split_dynamic_leaf(struct leaf_node *leaf, struct leaf_node *left, struct leaf_node *right)
{
	struct leaf_iterator iter = { 0 };
	char *leaf_buf = (char *)leaf;
	dl_init_leaf_iterator(leaf, &iter, NULL, -1);
	struct dl_slot_array *slot_array = dl_get_slot_array_offset(leaf);
	int32_t idx = 0;
	for (; idx < leaf->header.num_entries / 2; idx++) {
		if (!dl_is_leaf_iterator_valid(&iter)) {
			log_fatal("This should not happen, probably corruption?");
			assert(0);
			_exit(EXIT_FAILURE);
		}
		struct kv_splice_base splice = dl_leaf_iterator_curr(&iter);

		dl_append_splice_in_dynamic_leaf(
			left, &splice,
			kv_meta_is_tombstone((struct kv_splice_meta *)&leaf_buf[slot_array[iter.pos].index]));
		dl_leaf_iterator_next(&iter);
	}
	struct kv_splice_base pivot_splice = dl_get_general_splice(leaf, idx);
	for (; idx < leaf->header.num_entries; idx++) {
		if (!dl_is_leaf_iterator_valid(&iter)) {
			log_fatal("This should not happen, probably corruption?");
			_exit(EXIT_FAILURE);
		}
		struct kv_splice_base splice = dl_leaf_iterator_curr(&iter);

		dl_append_splice_in_dynamic_leaf(
			right, &splice,
			kv_meta_is_tombstone((struct kv_splice_meta *)&leaf_buf[slot_array[iter.pos].index]));
		dl_leaf_iterator_next(&iter);
	}
	return pivot_splice;
}

inline bool dl_is_reorganize_possible(struct leaf_node *leaf, int32_t kv_size)
{
	return leaf->header.fragmentation <= kv_size ? false : true;
}

void dl_reorganize_dynamic_leaf(struct leaf_node *leaf, struct leaf_node *target)
{
	char *leaf_buf = (char *)leaf;
	struct leaf_iterator iter = { 0 };
	dl_init_leaf_iterator(leaf, &iter, NULL, -1);
	struct dl_slot_array *slot_array = dl_get_slot_array_offset(leaf);
	for (int32_t i = 0; i < leaf->header.num_entries; i++) {
		if (!dl_is_leaf_iterator_valid(&iter)) {
			log_fatal("This should not happen, probably corruption?");
			_exit(EXIT_FAILURE);
		}
		struct kv_splice_base splice = dl_leaf_iterator_curr(&iter);
		dl_append_splice_in_dynamic_leaf(
			target, &splice,
			kv_meta_is_tombstone((struct kv_splice_meta *)&leaf_buf[slot_array[iter.pos].index]));
		dl_leaf_iterator_next(&iter);
	}
}

void dl_init_leaf_node(struct leaf_node *leaf, uint32_t leaf_size)
{
	_Static_assert(sizeof(struct dl_slot_array) == 2,
		       "Dynamic slot array is not 2 bytes, are you sure you want to continue?");
	memset(leaf, 0x00, leaf_size);
	dl_set_leaf_node_type(leaf, leafNode);
	leaf->header.log_size = leaf_size;
	leaf->header.node_size = leaf_size;
}

// cppcheck-suppress unusedFunction
uint32_t dl_leaf_get_node_size(struct leaf_node *leaf)
{
	return leaf->header.node_size;
}

inline void dl_set_leaf_node_type(struct leaf_node *leaf, nodeType_t node_type)
{
	leaf->header.type = node_type;
}

inline nodeType_t dl_get_leaf_node_type(struct leaf_node *leaf)
{
	return leaf->header.type;
}

// cppcheck-suppress unusedFunction
inline int32_t dl_get_leaf_num_entries(struct leaf_node *leaf)
{
	return leaf->header.num_entries;
}

// cppcheck-suppress unusedFunction
struct kv_splice_base dl_get_last_splice(struct leaf_node *leaf)
{
	struct kv_splice_base splice = { 0 };
	if (0 == leaf->header.num_entries)
		return splice;

	splice = dl_get_general_splice(leaf, leaf->header.num_entries - 1);
	return splice;
}
