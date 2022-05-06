#ifndef INDEX_NODE_H
#define INDEX_NODE_H
#include "btree.h"
#include <stdint.h>

#define NEW_INDEX_NODE_SIZE (INDEX_NODE_SIZE + KEY_BLOCK_SIZE)

struct pivot_key {
	uint32_t size;
	char data[];
} __attribute__((packed));

struct pivot_pointer {
	uint64_t child_offt;
};

struct new_index_node_iterator {
	struct new_index_node *node;
	struct pivot_key *key;
	int32_t position;
	int32_t num_entries;
	int8_t is_valid;
};

struct new_index_slot_array_entry {
	uint16_t pivot;
};

struct new_index_node {
	struct node_header header;
	char rest_space[NEW_INDEX_NODE_SIZE - sizeof(struct node_header)];
} __attribute__((packed));

enum add_guard_option_t { ADD_GUARD = 1, DO_NOT_ADD_GUARD };
/**
 * Initializes a freshly allocated index node. It initializes all of its fields
 * and inserts the guard zero key with node device offset set to UINT64_MAX.
 *
 * @param add_zero_guard: if set to a value greater than 0 inserts the zero
 * guard as pivot. The zero guard is the smallest possible key (aka pivot key
 * <size: 1, data: 0x00), with an initial device children offset set to
 * UINT64_MAX.
 *
 * @param node:  The node to be intialized.
 *
 * @param type: The node type, valid values are rootNode or internalNode.
*/
void new_index_init_node(enum add_guard_option_t option, struct new_index_node *node, nodeType_t type);

/**
  * Inserts a guard pivot (aka the smallest possible pivot key <size: 1, data:
  * 0x00) in the index node.
  *
  * @param node: The node to add the new guard.
  *
  * @param child_node_dev_offt: the child nodedevice offset that queries for
  * keys that are larger or equal to the guard will be forwarded.
*/
void new_index_add_guard(struct new_index_node *node, uint64_t child_node_dev_offt);

int new_index_is_full(struct new_index_node *node, struct pivot_key *key);

/*
 * Inserts a new pivot in the index node. When we split a leaf or an index node
 * we create two children left and right. Right contains all the keys greater
 * or equal to the pivot_key. Left contains all the keys that are greater or
 * equal of the previous pivot key
 * @return 0 on Success -1 on failure
 */

int new_index_insert_pivot(struct new_index_node *node, struct pivot_pointer *left_child, struct pivot_key *key,
			   struct pivot_pointer *right_child);

int new_index_append_pivot(struct new_index_node *node, struct pivot_key *key, struct pivot_pointer *right_child);
/*
 * Performs binary search in an index node and returns the device offt of the
 * children node that we need to follow
 */
uint64_t new_index_binary_search(struct new_index_node *node, void *lookup_key, enum KV_type lookup_key_format);

struct bt_rebalance_result new_index_split_node(struct new_index_node *node, bt_insert_req *ins_req);

/*
 * Iterators for parsing index nodes. Compaction, scanner, and other future
 * entiries must use this API in order to abstact the index node
 * implementation
 */
void new_index_iterator_init(struct new_index_node *node, struct new_index_node_iterator *iterator);

uint8_t new_index_iterator_is_valid(struct new_index_node_iterator *iterator);

struct pivot_key *new_index_node_iterator_get(struct new_index_node_iterator *iterator);

#endif
