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
#pragma once
#include "../btree/conf.h"
#include "device_structures.h"
#include "log_structures.h"
#include <pthread.h>
#include <stdint.h>

#define MEM_WORD_SIZE_IN_BITS (64UL)

struct mem_bitmap_word {
	uint64_t *word_addr;
	int word_id;
	uint8_t start_bit;
	uint8_t end_bit;
} __attribute__((packed, aligned(16)));
