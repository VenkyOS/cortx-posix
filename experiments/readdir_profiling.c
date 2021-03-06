/*
 * Filename: readdir_profiling.c
 * Description: Implementation tests for directory operartions
 *
 * Copyright (c) 2020 Seagate Technology LLC and/or its Affiliates
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Affero General Public License for more details.
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 * For any questions about this software or licensing,
 * please email opensource@seagate.com or cortx-questions@seagate.com. 
 */

/* This code has the implementation for the following experiment.
 * - Create files
 * - List files
 * - Delete files
 * - Calculate time taken for above operation
 * - NUM_FILES defines number of files to run the experiment for
*/
#include "ut_cortxfs_helper.h"
#define NUM_FILES 1000
#define MAX_FILENAME_LENGTH 16
#define DIR_ENV_FROM_STATE(__state) (*((struct ut_dir_env **)__state))

struct ut_dir_env {
	struct ut_cfs_params ut_cfs_obj;
	char **name_list;
	int entry_cnt;
};

struct readdir_ctx {
	int index;
	char *readdir_array[NUM_FILES+1];
};

/**
 * Call-back function for readdir
 */
static bool test_readdir_cb(void *ctx, const char *name, const cfs_ino_t *ino)
{
	struct readdir_ctx *readdir_ctx = ctx;

	readdir_ctx->readdir_array[readdir_ctx->index++] = strdup(name);

	return true;
}

/**
 * Free readdir array
 */
static void readdir_ctx_fini(struct readdir_ctx *ctx)
{
	int i;
	for (i = 0; i < ctx->index; i++) {
		free(ctx->readdir_array[i]);
	}
}

/**
 * Verify readdir content
 */
static void verify_dentries(struct readdir_ctx *ctx, struct ut_dir_env *env,
				int entry_start)
{
	int i;
	ut_assert_int_equal(env->entry_cnt, ctx->index);

	for (i = 0; i < ctx->index; i++) {
		ut_assert_string_equal(env->name_list[entry_start + i],
					ctx->readdir_array[i]);
	}
}

/**
 * Setup for dir_ops test group
 */
static int dir_ops_setup(void **state)
{
	int rc = 0;

	struct ut_dir_env *ut_dir_obj = calloc(sizeof(struct ut_dir_env), 1);
	ut_assert_not_null(ut_dir_obj);

	ut_dir_obj->name_list = calloc(sizeof(char *), NUM_FILES);
	if (ut_dir_obj->name_list == NULL) {
		rc = -ENOMEM;
		ut_assert_true(0);
	}

	*state = ut_dir_obj;
	rc = ut_cfs_fs_setup(state);

	ut_assert_int_equal(rc, 0);

	return rc;
}

/**
 * Teardown for dir_ops test group
 */
static int dir_ops_teardown(void **state)
{
	int rc = 0;
	struct ut_dir_env *ut_dir_obj = DIR_ENV_FROM_STATE(state);

	free(ut_dir_obj->name_list);
	rc = ut_cfs_fs_teardown(state);
	ut_assert_int_equal(rc, 0);

	free(*state);

	return rc;
}


static int create_files_teardown(void **state)
{
	int rc = 0, i=0;
	time_t start_time, end_time;

	struct ut_dir_env *ut_dir_obj = DIR_ENV_FROM_STATE(state);

	time(&start_time);
	printf("\nDeleting Files:Start time %s\n",ctime(&start_time));

	/*Delete files*/
	for (i=1;i<NUM_FILES+1;i++)
	{
		ut_dir_obj->ut_cfs_obj.file_name = ut_dir_obj->name_list[i];
		rc = ut_file_delete(state);
		ut_assert_int_equal(rc, 0);
	}

	/*Delete Directory*/
	ut_dir_obj->ut_cfs_obj.parent_inode = CFS_ROOT_INODE;
	ut_dir_obj->ut_cfs_obj.file_name = ut_dir_obj->name_list[0];
	rc = ut_dir_delete(state);
	ut_assert_int_equal(rc,0);

	time(&end_time);
	printf("Deleting Files:End time %s\n",ctime(&end_time));

	for (i=0;i<NUM_FILES+1;i++)
	{
		free(ut_dir_obj->name_list[i]);
	}

	return rc;
}

/**Test to create files**/
static int create_files_setup(void **state)
{
	int i=0,rc=0;
	time_t start_time, end_time;
	struct ut_dir_env *ut_dir_obj = DIR_ENV_FROM_STATE(state);

	ut_dir_obj->entry_cnt = NUM_FILES;

	/* Allocate space for NUM_FILES files, plus 1 directory entry */
	for (i=0;i<NUM_FILES+1;i++)
	{
		ut_dir_obj->name_list[i]=malloc(MAX_FILENAME_LENGTH);
		if (ut_dir_obj->name_list == NULL){
			rc = -ENOMEM;
			ut_assert_true(0);
		}
	}

	/* Create a directory under root*/
	strncpy(ut_dir_obj->name_list[0], "Test_Dir",strlen("Test_Dir")+1);
	printf("Test dir %s\n",ut_dir_obj->name_list[0]);
	ut_dir_obj->ut_cfs_obj.file_name = ut_dir_obj->name_list[0];
	rc = ut_dir_create(state);
	ut_assert_int_equal(rc,0);

	time(&start_time);
	printf("\nStart time:Create %d files is %s\n", NUM_FILES,ctime(&start_time));

	ut_dir_obj->ut_cfs_obj.parent_inode = ut_dir_obj->ut_cfs_obj.file_inode;

	/* Create files under Test Directory*/
	for (i=1;i<NUM_FILES+1;i++)
	{
		snprintf(ut_dir_obj->name_list[i],MAX_FILENAME_LENGTH,"%d",i+1);
		ut_dir_obj->ut_cfs_obj.file_name = ut_dir_obj->name_list[i];
		rc = ut_file_create(state);
		ut_assert_int_equal(rc,0);
	}
	time(&end_time);
	printf("End time:Create %d files is %s\n", NUM_FILES, ctime(&end_time));
	return rc;
}

static void read_files(void **state)
{
	int rc = 0;
	time_t start_time, end_time;
	cfs_ino_t dir_inode = 0LL;

	struct ut_dir_env *ut_dir_obj = DIR_ENV_FROM_STATE(state);
	struct ut_cfs_params *ut_cfs_obj = &ut_dir_obj->ut_cfs_obj;

	struct readdir_ctx readdir_ctx[1] = {{
		.index = 0,
	}};

	time(&start_time);
	printf("\nStart time:Lookup %s\n",ctime(&start_time));

	rc = cfs_lookup(ut_cfs_obj->cfs_fs, &ut_cfs_obj->cred,
			&ut_cfs_obj->current_inode, ut_dir_obj->name_list[0],
			&dir_inode);
	ut_assert_int_equal(rc,0);
	time(&end_time);
	printf("End time:Lookup %s\n", ctime(&end_time));

	time(&start_time);
	printf("\nStart time:Read %d files %s\n", NUM_FILES, ctime(&start_time));
	rc = cfs_readdir(ut_cfs_obj->cfs_fs, &ut_cfs_obj->cred, &dir_inode,
			test_readdir_cb, readdir_ctx);
	ut_assert_int_equal(rc, 0);
	time(&end_time);
	printf("End time:Read %d files %s\n", NUM_FILES, ctime(&end_time));
	verify_dentries(readdir_ctx, ut_dir_obj, 1);

	readdir_ctx_fini(readdir_ctx);
}
int main(void)
{
	int rc = 0;
	char *test_log = "/var/log/cortx/test/ut/ut_cortxfs.log";

	printf("Directory tests\n");

	rc = ut_load_config(CONF_FILE);
	if (rc != 0) {
		printf("ut_load_config: err = %d\n", rc);
		goto end;
	}

	test_log = ut_get_config("cortxfs", "log_path", test_log);

	rc = ut_init(test_log);
	if (rc != 0) {
		printf("ut_init failed, log path=%s, rc=%d.\n", test_log, rc);
		goto out;
	}

	struct test_case test_list[] = {
		ut_test_case(read_files, create_files_setup, create_files_teardown),
	};

	int test_count = sizeof(test_list)/sizeof(test_list[0]);
	int test_failed = 0;

	test_failed = ut_run(test_list, test_count, dir_ops_setup,
			dir_ops_teardown);

	ut_fini();

	ut_summary(test_count, test_failed);

out:
	free(test_log);

end:
	return rc;
}
