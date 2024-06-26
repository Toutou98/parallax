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

#ifndef DYNAMIC_LEAF_H
#define DYNAMIC_LEAF_H
#include "btree_node.h"
#include "kv_pairs.h"
#include <stdbool.h>
#include <stdint.h>
struct leaf_node;

struct leaf_iterator {
	struct kv_splice_base splice;
	struct leaf_node *leaf;
	int pos;
};
/**
 * @brief Inserts (in sorted orderd) a kv_splice_base in the leaf node.
 * @param leaf pointer to the actual leaf node
 * @param splice pointer to the splice object
 * @param is_tombstone indicates if the splice is a tombstone
 * @param exact_match pointer to a boolean flag. If there is an exact match it
 * @returns true on success false on failure. The operation may fail
 * if the leaf node does not have adequate space to store the splice
 */
bool dl_insert_in_dynamic_leaf(struct leaf_node *leaf, struct kv_splice_base *splice, bool is_tombstone,
			       bool *exact_match);

/**
 * @brief Appends a kv_splice_base in the leaf node.
 * @param leaf pointer to the actual leaf node
 * @param general_splice pointer to the splice object
 * @param is_tombstone indicates if the splice is a tombstone
 * @returns true on success false on failure. The operation may fail
 * if the leaf node does not have adequate space to store the splice
 */
bool dl_append_splice_in_dynamic_leaf(struct leaf_node *leaf, struct kv_splice_base *general_splice, bool is_tombstone);

/**
 * @brief Returns a kv splice associated with the key.
 * @param leaf pointer to the leaf node
 * @param key pointer to the key
 * @param key_size size of the key in bytes
 * @param error points to an error message in case of a failure.
 * @returns On success (exact match) it returns a kv_splice_base object and
 * sets *error to NULL. If the key is not found inside the leaf or in case of
 * other error it sets *error to the appropriate message.
 */
struct kv_splice_base dl_find_kv_in_dynamic_leaf(struct leaf_node *leaf, const char *key, int32_t key_size,
						 const char **error);

/**
 * @brief Splits the contents of leaf node equally to left and right nodes. In
 * particular, it will place the first N/2 entries of leaf to left and the
 * remaining to the right. If left and right accidentally have any content it
 * will be erased by this operation.
 * @param leaf pointer to the leaf node
 * @param left pointer to the left the node
 * @param right pointer to the right node
 * @returns On success it returns a kv_splice_base with the middle key that
 * btree uses as a pivot to the index node. Caution the kv_general_spice
 * contains internaly a reference to the KV pair. The application must copy the
 * contents of the splice in the index node. On failure it returns a zeroed
 * kv_splice_base.
 */
struct kv_splice_base dl_split_dynamic_leaf(struct leaf_node *leaf, struct leaf_node *left, struct leaf_node *right);
/**
 * @brief Returns if the overflow in the leaf to insert a kv_size splice can be
 * handled through reorganization of the leaf due to fragmented space.
 * @param leaf pointer to the leaf node
 * @param kv_size the size of the splice that causes the overflow.
 * @return true if the leaf can host the splice after reorganization otherwise
 * false.
 */
bool dl_is_reorganize_possible(struct leaf_node *leaf, int32_t kv_size);

/**
 * @brief Copies the valid kv splices to the target leaf node. If target leaf
 * node accidentally has contents it will be erased by this operation.
 * @param leaf pointer to the leafd node
 * @param target pointer to the target node
 */
void dl_reorganize_dynamic_leaf(struct leaf_node *leaf, struct leaf_node *target);

/**
 * @brief Initializes the contents of the leaf node. If the leaf node has data
 * they will be erased.
 * @param leaf node pointer to the leaf
 * @param leaf_size the size of the leaf node
 */
void dl_init_leaf_node(struct leaf_node *leaf, uint32_t leaf_size);

/**
 * @brief Returns if the leaf can store kv_splice of kv_size or it will cause
 * an overflow.
 * @param leaf pointer to the leaf node.
 * @param kv_size the size of the kv_splice that we want to store.
 * @returns true if the leaf has adequate space to store the kv pair otherwise
 * false.
 */
bool dl_is_leaf_full(struct leaf_node *leaf, uint32_t kv_size);

/**
 * @brief Returns the position inside the leaf where the key should by inserted.
 * @param leaf pointer to the leaf node
 * @param key pointer to the key
 * @param key_size size of the key in bytes
 * @param exact_match pointer to a boolean flag. If there is an exact match it
 * sets it to true otherwise it sets it to false.
 */
int32_t dl_search_get_pos(struct leaf_node *leaf, const char *key, int32_t key_size, bool *exact_match);

/**
 * @brief Returns the splice store at the specified position.
 * @param leaf pointer to the leaf node
 * @param position the position which splice we want to retrieve
 */
struct kv_splice_base dl_get_general_splice(struct leaf_node *leaf, int32_t position);
/**
 * @brief Sets the type of the node
 * @param leaf pointer to the leaf node
 * @param node_type the type of the node
 */
void dl_set_leaf_node_type(struct leaf_node *leaf, nodeType_t node_type);

/**
 * @brief Returns the type of the node
 * @param leaf pointer to the leaf node
 */
nodeType_t dl_get_leaf_node_type(struct leaf_node *leaf);

/**
 * @brief Returns the number of the entries in the leaf
 * @param leaf pointer to the leaf node
 */
int32_t dl_get_leaf_num_entries(struct leaf_node *leaf);

/**
 * @brief initializes a dynamic leaf cursor at the position of the key provided. To get the first key in leaf
 * key must be NULL and key_size = -1
 * @param leaf: pointer to the leaf node
 * @param iter: pointer to a leaf iterator
 * @param key: key from which the iterator will begin its traversal
 * @param key_size: the size of the key
 */
void dl_init_leaf_iterator(struct leaf_node *leaf, struct leaf_iterator *iter, const char *key, int32_t key_size);

/**
 * @brief function returning if the iterator is within bounds
 * @param iter: pointer to a leaf iterator
 */
bool dl_is_leaf_iterator_valid(struct leaf_iterator *iter);

/**
 * @brief proceeds the iterator to the next key, this should be checked with the assocated valid function aswell
 * @param iter: pointer to a leaf iterator
 */
void dl_leaf_iterator_next(struct leaf_iterator *iter);

/**
 * @brief retursn the current kv pointed by the iterator
 * @param iter: pointer to a leaf iterator
 */
struct kv_splice_base dl_leaf_iterator_curr(struct leaf_iterator *iter);

/**
 * @brief Returns the node size of the leaf
 * @param leaf: pointers to a leaf structure
 */
uint32_t dl_leaf_get_node_size(struct leaf_node *leaf);

/**
* @brief Returns the last splice present on the leaf
  * @param leaf pointer to the leaf node
  * @return The last splice
*/
struct kv_splice_base dl_get_last_splice(struct leaf_node *leaf);
#endif
