#pragma once

#include <stddef.h>
#include <stdio.h>

#define TEST_BAD_FAIL_EXIT_CODE 99

extern const struct _test_config_t {
	int (*test_init_fnc)(void);
	int (*test_fini_fnc)(void);
	int (*test_fnc)(void);
	unsigned threads_count;
	const char *expected_output;
	size_t expected_output_size;
	const char *test_name;
	const char **test_arguments;
} test_config;

extern FILE *test_output_file;

extern void test_abort(void);
extern int test_thread_barrier(void);

// core.c mock
typedef unsigned rid_t;
typedef unsigned nid_t;

extern unsigned n_nodes;
extern unsigned n_threads;
extern nid_t nid;
extern __thread rid_t rid;
