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
#include "lsn.h"
#include <stdint.h>

inline size_t get_lsn_size(void)
{
	return sizeof(struct lsn);
}

inline int64_t compare_lsns(const struct lsn *left, const struct lsn *right)
{
	return left->id - right->id;
}

inline int64_t get_lsn_id(const struct lsn *lsn)
{
	return lsn->id;
}

inline void set_lsn_id(struct lsn *lsn, int64_t ticket)
{
	lsn->id = ticket;
}

inline void reset_lsn(struct lsn *lsn)
{
	lsn->id = 0;
}

inline struct lsn_factory lsn_factory_init(int64_t starting_ticket)
{
	struct lsn_factory new_lsn_factory = { .ticket_id = starting_ticket };
	return new_lsn_factory;
}

// cppcheck-suppress unusedFunction
inline void reset_lsn_factory(struct lsn_factory *lsn_factory)
{
	lsn_factory->ticket_id = 0;
}

inline int64_t lsn_factory_increase_ticket(struct lsn_factory *lsn_factory)
{
	return __sync_fetch_and_add(&lsn_factory->ticket_id, 1);
}

inline struct lsn increase_lsn(struct lsn_factory *lsn_factory)
{
	struct lsn new_lsn = { .id = lsn_factory_increase_ticket(lsn_factory) };
	return new_lsn;
}

inline int64_t lsn_factory_get_ticket(const struct lsn_factory *lsn_factory)
{
	return lsn_factory->ticket_id;
}

struct lsn get_max_lsn(void)
{
	struct lsn max_lsn = { .id = INT64_MAX };
	return max_lsn;
}
