#include "arg_parser.h"
#include "common/common.h"
#include <allocator/volume_manager.h>
#include <assert.h>
#include <btree/btree.h>
#include <fcntl.h>
#include <getopt.h>
#include <linux/fs.h>
#include <log.h>
#include <parallax/parallax.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define KV_SIZE 1500
#define KEY_PREFIX "ts"
#define NUM_KEYS num_keys
#define TOTAL_KEYS 1000000
uint64_t num_keys = 100000;

typedef struct key {
	uint32_t key_size;
	char key_buf[];
} key;

typedef struct value {
	uint32_t value_size;
	char value_buf[];
} value;

void serially_insert_keys(par_handle hd)
{
	uint64_t i;
	key *k = (key *)malloc(KV_SIZE);
	struct par_key_value key_value;

	log_info("Starting population for %lu keys...", NUM_KEYS);

	for (i = TOTAL_KEYS; i < (TOTAL_KEYS + NUM_KEYS); i++) {
		memcpy(k->key_buf, KEY_PREFIX, strlen(KEY_PREFIX));
		sprintf(k->key_buf + strlen(KEY_PREFIX), "%llu", (long long unsigned)i);
		k->key_size = strlen(k->key_buf) + 1;
		value *v = (value *)((uint64_t)k + sizeof(key) + k->key_size);
		v->value_size = KV_SIZE - ((2 * sizeof(key)) + k->key_size);
		memset(v->value_buf, 0xDD, v->value_size);
		if (i % 10000 == 0) {
			log_info("%s", k->key_buf);
		}

		key_value.k.size = k->key_size;
		key_value.k.data = k->key_buf;
		key_value.v.val_buffer = v->value_buf;
		key_value.v.val_size = v->value_size;
		const char *error_message = NULL;
		par_put(hd, &key_value, &error_message);
		if (error_message) {
			log_fatal("Put failed %s", error_message);
			BUG_ON();
		}
	}

	free(k);
	log_info("Population ended");
}

void get_all_keys(par_handle hd)
{
	uint64_t i;
	key *k = (key *)calloc(1, KV_SIZE);
	struct par_key par_key;

	log_info("Search for all keys");

	for (i = TOTAL_KEYS; i < (TOTAL_KEYS + NUM_KEYS); i++) {
		memcpy(k->key_buf + 4, KEY_PREFIX, strlen(KEY_PREFIX));
		sprintf(k->key_buf + 4 + strlen(KEY_PREFIX), "%llu", (long long unsigned)i);
		k->key_size = strlen(&k->key_buf[4]) + 1;
		*(uint32_t *)k->key_buf = k->key_size;
		/* log_info("size %u, %u , string %*s", *(uint32_t*) k->key_buf,k->key_size,k->key_size,&k->key_buf[4]); */
		if (i % 10000 == 0) {
			log_info("%s", &k->key_buf[4]);
		}

		par_key.data = &k->key_buf[4];
		par_key.size = k->key_size;

		if (par_exists(hd, &par_key) != PAR_SUCCESS) {
			log_fatal("ERROR key not found!");
			exit(EXIT_FAILURE);
		}
	}
	free(k);
	log_info("Searching finished");
}

void delete_half_keys(par_handle hd)
{
	key *k = (key *)calloc(1, KV_SIZE);

	log_info("Delete started");
	for (uint64_t i = TOTAL_KEYS; i < (TOTAL_KEYS + NUM_KEYS / 2); i++) {
		struct par_key par_key = { 0 };
		memcpy(k->key_buf, KEY_PREFIX, strlen(KEY_PREFIX));
		sprintf(k->key_buf + strlen(KEY_PREFIX), "%llu", (long long unsigned)i);
		k->key_size = strlen(k->key_buf) + 1;
		/* log_info("size %u, %u , string %*s", *(uint32_t*) k->key_buf,k->key_size,k->key_size,&k->key_buf[4]); */
		par_key.data = k->key_buf;
		par_key.size = k->key_size;

		if (i % 10000 == 0) {
			log_info("%s", k->key_buf);
		}

		const char *error_message = NULL;
		par_delete(hd, &par_key, &error_message);
		if (error_message) {
			log_fatal("key %s not found!", error_message);
			_exit(EXIT_FAILURE);
		}
	}
	free(k);
	log_info("Success! Delete finished");
}

void get_all_valid_keys(par_handle hd)
{
	key *k = (key *)calloc(1, KV_SIZE);
	uint64_t num_of_keys_not_found = 0;

	log_info("Search for all keys");
	for (uint64_t i = TOTAL_KEYS; i < (TOTAL_KEYS + NUM_KEYS / 2); i++) {
		struct par_key par_key = { 0 };
		memcpy(k->key_buf + 4, KEY_PREFIX, strlen(KEY_PREFIX));
		sprintf(k->key_buf + 4 + strlen(KEY_PREFIX), "%llu", (long long unsigned)i);
		k->key_size = strlen(&k->key_buf[4]) + 1;
		*(uint32_t *)k->key_buf = k->key_size;
		/* log_info("size %u, %u , string %*s", *(uint32_t*) k->key_buf,k->key_size,k->key_size,&k->key_buf[4]); */
		if (i % 10000 == 0) {
			log_info("%s", &k->key_buf[4]);
		}

		par_key.data = &k->key_buf[4];
		par_key.size = k->key_size;
		if (par_exists(hd, &par_key) == PAR_KEY_NOT_FOUND) {
			/* log_info("%d", get_op.tombstone); */
			/* BREAKPOINT; */
			++num_of_keys_not_found;
		}

		/* assert(get_op.tombstone); */
		/* assert(get_op.found); */
	}
	for (uint64_t i = ((TOTAL_KEYS + NUM_KEYS / 2)); i < (TOTAL_KEYS + NUM_KEYS); i++) {
		struct par_key par_key = { 0 };
		memcpy(k->key_buf + 4, KEY_PREFIX, strlen(KEY_PREFIX));
		sprintf(k->key_buf + 4 + strlen(KEY_PREFIX), "%llu", (long long unsigned)i);
		k->key_size = strlen(&k->key_buf[4]) + 1;
		*(uint32_t *)k->key_buf = k->key_size;
		/* log_info("size %u, %u , string %*s", *(uint32_t*) k->key_buf,k->key_size,k->key_size,&k->key_buf[4]); */
		if (i % 10000 == 0) {
			log_info("%s", &k->key_buf[4]);
		}

		par_key.data = &k->key_buf[4];
		par_key.size = k->key_size;
		if (par_exists(hd, &par_key) == PAR_KEY_NOT_FOUND)
			num_of_keys_not_found++;

		/* assert(get_op.tombstone); */
		/* assert(get_op.found); */
	}

	free(k);

	if (num_of_keys_not_found != NUM_KEYS / 2) {
		log_fatal("Searching finished not found keys %lu out of a population of %lu", num_of_keys_not_found,
			  NUM_KEYS);
		_exit(EXIT_FAILURE);
	}
}

void scan_all_valid_keys(par_handle hd)
{
	uint64_t i;
	key *k = (key *)calloc(1, KV_SIZE);
	uint64_t count = 0;
	struct par_key par_key;
	par_scanner my_scanner = NULL;

	memset(&my_scanner, 0, sizeof(my_scanner));

	log_info("Scan for all valid keys");
	for (i = ((TOTAL_KEYS + ((NUM_KEYS / 2)))); i < (TOTAL_KEYS + NUM_KEYS); i++) {
		memcpy(k->key_buf + 4, KEY_PREFIX, strlen(KEY_PREFIX));
		sprintf(k->key_buf + 4 + strlen(KEY_PREFIX), "%llu", (long long unsigned)i);
		k->key_size = strlen(&k->key_buf[4]) + 1;
		*(uint32_t *)k->key_buf = k->key_size;
		/* log_info("size %u, %u , string %*s", *(uint32_t*) k->key_buf,k->key_size,k->key_size,&k->key_buf[4]); */
		if (i % 10000 == 0) {
			log_info("%s", &k->key_buf[4]);
		}

		if (!my_scanner) {
			par_key.size = k->key_size;
			par_key.data = &k->key_buf[4];
			const char *error_message = NULL;
			my_scanner = par_init_scanner(hd, &par_key, PAR_GREATER_OR_EQUAL, &error_message);

			if (!par_is_valid(my_scanner)) {
				log_fatal("Nothing found! it shouldn't!");
				exit(EXIT_FAILURE);
			}
		}
		struct par_key my_keyptr = par_get_key(my_scanner);
		//log_info("key is %d:%s  malloced %d scanner size %d",k->key_size,k->key_buf,sc->malloced,sizeof(scannerHandle));
		//log_info("key of scanner %d:%s",*(uint32_t *)sc->keyValue,sc->keyValue + sizeof(uint32_t));
		if (memcmp(&k->key_buf[4], my_keyptr.data, my_keyptr.size) != 0) {
			log_fatal("Test failed key %s not found scanner instead returned %d:%s", &k->key_buf[4],
				  my_keyptr.size, my_keyptr.data);
			exit(EXIT_FAILURE);
		} else
			++count;
		/* assert(get_op.tombstone); */
		/* assert(get_op.found); */
		if (!par_get_next(my_scanner))
			break;
	}
	if ((NUM_KEYS / 2) != count) {
		log_fatal("Test failed found %lu keys should have found: %lu", count, NUM_KEYS / 2);
		exit(EXIT_FAILURE);
	}
	par_close_scanner(my_scanner);

	free(k);
	log_info("Scanning finished %lu", count);
	assert(count == NUM_KEYS / 2);
}

int main(int argc, char *argv[])
{
	int help_flag = 0;
	struct wrap_option options[] = {
		{ { "help", no_argument, &help_flag, 1 }, "Prints valid arguments for test_medium.", NULL, INTEGER },
		{ { "file", required_argument, 0, 'a' },
		  "--file=path to file of db, parameter that specifies the target where parallax is going to run.",
		  NULL,
		  STRING },
		{ { "num_of_kvs", required_argument, 0, 'b' },
		  "--num_of_kvs=number, parameter that specifies the number of operations the test will execute.",
		  NULL,
		  INTEGER },
		{ { 0, 0, 0, 0 }, "End of arguments", NULL, INTEGER }
	};
	unsigned options_len = (sizeof(options) / sizeof(struct wrap_option));

	arg_parse(argc, argv, options, options_len);
	arg_print_options(help_flag, options, options_len);

	num_keys = *(int *)get_option(options, 2);

	par_db_options db_options = { .volume_name = get_option(options, 1),
				      .create_flag = PAR_CREATE_DB,
				      .db_name = "test.db",
				      .options = par_get_default_options() };
	const char *error_message = NULL;
	par_handle handle = par_open(&db_options, &error_message);

	if (error_message) {
		log_fatal("%s", error_message);
		return EXIT_FAILURE;
	}

	serially_insert_keys(handle);
	get_all_keys(handle);
	delete_half_keys(handle);
	get_all_valid_keys(handle);
	error_message = par_close(handle);

	if (error_message) {
		log_fatal("%s", error_message);
		return EXIT_FAILURE;
	}

	log_info("----------------------------------CLOSE FINISH--------------------------------------");
	handle = par_open(&db_options, &error_message);
	if (error_message) {
		log_fatal("par_open() failed with message: %s", error_message);
		return EXIT_FAILURE;
	}

	get_all_valid_keys(handle);
	scan_all_valid_keys(handle);
	error_message = par_close(handle);

	if (error_message) {
		log_fatal("%s", error_message);
		return EXIT_FAILURE;
	}
	return 0;
}
