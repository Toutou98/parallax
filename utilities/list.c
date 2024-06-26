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
#include "list.h"
#include "../lib/btree/conf.h"
#include <log.h>
#include <stdlib.h>
#include <string.h>

struct klist *klist_init(void)
{
	struct klist *list = (struct klist *)calloc(1, sizeof(struct klist));
	if (!list) {
		log_fatal("Calloc failed");
		exit(EXIT_FAILURE);
	}
	MUTEX_INIT(&list->list_lock, NULL);
	list->size = 0;
	list->first = NULL;
	list->last = NULL;
	return list;
}

void *klist_get_first(struct klist *list)
{
	struct klist_node *node = NULL;

	if (list) {
		MUTEX_LOCK(&list->list_lock);
		node = list->first;
		MUTEX_UNLOCK(&list->list_lock);
	}
	return node;
}

void klist_add_first(struct klist *list, void *data, const char *data_key, destroy_node_data destroy_data)
{
	if (!list)
		return;

	MUTEX_LOCK(&list->list_lock);

	struct klist_node *node = (struct klist_node *)calloc(1, sizeof(struct klist_node));
	if (!node) {
		log_fatal("Calloc failed out of memory");
		exit(EXIT_FAILURE);
	}
	node->data = data;
	if (data_key) {
		node->key = calloc(1, strlen(data_key) + 1);
		if (!node->key) {
			log_fatal("Calloc failed out of memory");
			exit(EXIT_FAILURE);
		}
		strcpy(node->key, data_key);
	}
	node->destroy_data = destroy_data;
	if (list->size == 0) {
		node->prev = NULL;
		node->next = NULL;
		list->first = node;
		list->last = node;
	} else {
		node->prev = NULL;
		node->next = list->first;
		list->first->prev = node;
		list->first = node;
	}
	++list->size;
	MUTEX_UNLOCK(&list->list_lock);
}

int klist_remove_element(struct klist *list, const void *data)
{
	struct klist_node *node = NULL;
	int ret = 0;
	MUTEX_LOCK(&list->list_lock);
	node = list->first;
	for (int i = 0; i < list->size; i++) {
		if (node->data == data) {
			if (node->next != NULL) /*node is not the last*/
				node->next->prev = node->prev;
			else
				list->last = node->prev;
			if (node->prev != NULL) /*node is not the first*/
				node->prev->next = node->next;
			else
				list->first = node->next;
			--list->size;
			//if (node->key)
			//	free(node->key);
			//free(node);
			ret = 1;
		}
		node = node->next;
	}
	MUTEX_UNLOCK(&list->list_lock);
	return ret;
}

void *klist_find_element_with_key(struct klist *list, const char *data_key)
{
	void *data = NULL;
	MUTEX_LOCK(&list->list_lock);
	struct klist_node *node = list->first;
	for (int i = 0; i < list->size; i++) {
		if (strcmp(node->key, data_key) == 0)
			data = node->data;
		node = node->next;
	}
	MUTEX_UNLOCK(&list->list_lock);
	return data;
}
