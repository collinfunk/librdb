#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include "test_common.h"

static void test_createReader_missingFile(void **state) {
    UNUSED(state);

    RdbParser *parser = RDB_createParserRdb(NULL);
    RdbxReaderFile *reader = RDBX_createReaderFile(parser, "./test/dumps/non_exist_file.rdb");

    /* verify didn't get back reader instance */
    assert_null(reader);

    /* verify returned error code */
    RdbRes err = RDB_getErrorCode(parser);
    assert_int_equal(err, RDB_ERR_FAILED_OPEN_RDB_FILE);

    /* verify returned error string */
    assert_true(strstr(RDB_getErrorMessage(parser), "Failed to open RDB file"));
    RDB_deleteParser(parser);
}

static void test_empty_rdb(void **state) {
    UNUSED(state);

    const char *rdbfile = DUMP_FOLDER("empty.rdb");
    const char *jsonfile = TMP_FOLDER("empty.json");

    RdbStatus  status;
    RdbParser *parser = RDB_createParserRdb(NULL);
    RDB_setLogLevel(parser, RDB_LOG_ERR);
    assert_non_null(RDBX_createReaderFile(parser, rdbfile));
    RdbxToJsonConf r2jConf = {
            .level = RDB_LEVEL_DATA,
            .encoding = RDBX_CONV_JSON_ENC_PLAIN,
            .includeAuxField = 1,
            .includeFunc = 0,
            .includeStreamMeta = 0,
            .flatten = 1,
    };

    assert_non_null(RDBX_createHandlersToJson(parser,
                                              jsonfile,
                                              &r2jConf));

    while ((status = RDB_parse(parser)) == RDB_STATUS_WAIT_MORE_DATA);
    assert_int_equal( status, RDB_STATUS_OK);

    RDB_deleteParser(parser);
}

static void test_not_support_future_rdb_version(void **state) {
    UNUSED(state);

    const char *rdbfile = DUMP_FOLDER("future_v19.rdb");
    const char *jsonfile = TMP_FOLDER("future_v19.json");

    RdbStatus  status;
    RdbParser *parser = RDB_createParserRdb(NULL);
    RDB_setLogLevel(parser, RDB_LOG_ERR);
    assert_non_null(RDBX_createReaderFile(parser, rdbfile));
    RdbxToJsonConf r2jConf = { .level = RDB_LEVEL_DATA };

    assert_non_null(RDBX_createHandlersToJson(parser,
                                              jsonfile,
                                              &r2jConf));

    while ((status = RDB_parse(parser)) == RDB_STATUS_WAIT_MORE_DATA);
    assert_int_equal( status, RDB_STATUS_ERROR);

    /* verify returned error code */
    RdbRes err = RDB_getErrorCode(parser);
    assert_int_equal(err, RDB_ERR_UNSUPPORTED_RDB_VERSION);

    RDB_deleteParser(parser);
}

static void test_mixed_levels_registration(void **state) {
    UNUSED(state);
    const char *rdbfile = DUMP_FOLDER("multiple_lists_strings.rdb");
    const char *jsonfileData = TMP_FOLDER("multiple_lists_strings_data.json");
    const char *jsonfileRaw = TMP_FOLDER("multiple_lists_strings_raw.json");

    RdbStatus  status;
    RdbParser *parser = RDB_createParserRdb(NULL);
    RDB_setLogLevel(parser, RDB_LOG_ERR);
    assert_non_null(RDBX_createReaderFile(parser, rdbfile));
    RdbxToJsonConf conf1 = {RDB_LEVEL_DATA, RDBX_CONV_JSON_ENC_PLAIN, 0, 0, 0, 0, 1};
    assert_non_null(RDBX_createHandlersToJson(parser, jsonfileData, &conf1));

    RdbxToJsonConf conf2 = {RDB_LEVEL_RAW, RDBX_CONV_JSON_ENC_PLAIN, 0, 0, 0, 0, 1};
    assert_non_null(RDBX_createHandlersToJson(parser, jsonfileRaw, &conf2));

    /* configure at what level of the parser each obj type should be handled and callback */
    RDB_handleByLevel(parser, RDB_DATA_TYPE_STRING, RDB_LEVEL_RAW);
    RDB_handleByLevel(parser, RDB_DATA_TYPE_LIST, RDB_LEVEL_DATA);

    while ((status = RDB_parse(parser)) == RDB_STATUS_WAIT_MORE_DATA);
    assert_int_equal( status, RDB_STATUS_OK);

    RDB_deleteParser(parser);

    assert_json_equal(jsonfileRaw, DUMP_FOLDER("multiple_lists_strings_subset_str.json"), 1);
    assert_json_equal(jsonfileData, DUMP_FOLDER("multiple_lists_strings_subset_list.json"), 1);
}

static void test_checksum(void **state) {
    RdbParser *parser;
    RdbStatus  status;
    UNUSED(state);
    const char *rdbfile = DUMP_FOLDER("invalid_chksum_v8.rdb");

    /* fail on checksum error */
    parser = RDB_createParserRdb(NULL);
    RDB_setLogLevel(parser, RDB_LOG_ERR);
    assert_non_null(RDBX_createReaderFile(parser, rdbfile));
    while ((status = RDB_parse(parser)) == RDB_STATUS_WAIT_MORE_DATA);
    assert_int_equal( status, RDB_STATUS_ERROR);
    assert_int_equal(RDB_getErrorCode(parser), RDB_ERR_CHECKSUM_FAILURE);
    RDB_deleteParser(parser);

    /* ignore checksum error */
    parser = RDB_createParserRdb(NULL);
    assert_non_null(RDBX_createReaderFile(parser, rdbfile));
    RDB_IgnoreChecksum(parser);
    while ((status = RDB_parse(parser)) == RDB_STATUS_WAIT_MORE_DATA);
    assert_int_equal( status, RDB_STATUS_OK);
    RDB_deleteParser(parser);
}

/* Test opcode relevant only to Redis Ent (Does nothing) */
static void __test_opcode_ram_lru(void **state) {
    UNUSED(state);

    const char *rdbfile = DUMP_FOLDER("redis_ent_opcode_ram_lru.rdb");
    //const char *jsonfile = TMP_FOLDER("empty.json");

    RdbStatus  status;
    RdbParser *parser = RDB_createParserRdb(NULL);
    RDB_setLogLevel(parser, RDB_LOG_ERR);
    assert_non_null(RDBX_createReaderFile(parser, rdbfile));
    
    while ((status = RDB_parse(parser)) == RDB_STATUS_WAIT_MORE_DATA);
    assert_int_equal( status, RDB_STATUS_OK);

    RDB_deleteParser(parser);
}

static void test_examples(void **state) {
    UNUSED(state);
    runSystemCmd("make example > /dev/null ");
}

RdbRes handle_start_rdb_report_long_errors(RdbParser *p, void *userData, int rdbVersion) {
    UNUSED(userData, rdbVersion);
    for (int i = 2 ; i < 1000; i++)
        RDB_reportError(p, (RdbRes) i, "Error Report number:%d", i);
    return 1001; /* This value will be eventually returned as the error code */
}

/* Test a faulty parser handler that reports on 999 errors. Only the first 
 * errors are reported, and the last one is kept as well. */ 
static void test_report_long_error(void **state) {
    RdbStatus  status;
    UNUSED(state);
    void *user_data = NULL;

    RdbHandlersRawCallbacks cb = { .handleStartRdb = handle_start_rdb_report_long_errors };
    RdbParser *parser = RDB_createParserRdb(NULL);
    RDB_setLogger(parser, dummyLogger);
    assert_non_null(RDBX_createReaderFile(parser, "./test/dumps/quicklist2_v11.rdb"));
    assert_non_null(RDB_createHandlersRaw(parser, &cb, user_data, NULL));


    while ((status = RDB_parse(parser)) == RDB_STATUS_WAIT_MORE_DATA);
    assert_int_equal(status, RDB_STATUS_ERROR);
    const char *returned = RDB_getErrorMessage(parser);
    const char *expected =
            "[errcode=2] [elementRdbHeader::State=0] Error Report number:2\n"
            "[errcode=3] [elementRdbHeader::State=0] Error Report number:3\n[errcode=4] [elementRdbHeader::State=0] Error Report number:4\n"
            "[errcode=5] [elementRdbHeader::State=0] Error Report number:5\n[errcode=6] [elementRdbHeader::State=0] Error Report number:6\n"
            "[errcode=7] [elementRdbHeader::State=0] Error Report number:7\n[errcode=8] [elementRdbHeader::State=0] Error Report number:8\n"
            "[errcode=9] [elementRdbHeader::State=0] Error Report number:9\n[errcode=10] [elementRdbHeader::State=0] Error Report number:10\n"
            "[errcode=11] [elementRdbHeader::State=0] Error Report number:11\n[errcode=12] [elementRdbHeader::State=0] Error Report number:12\n"
            "[errcode=13] [elementRdbHeader::State=0] Error Report number:13\n[errcode=14] [elementRdbHeader::State=0] Error Report number:14\n"
            "[errcode=15] [elementRdbHeader::State=0] Error Report number:15\n[errcode=16] [elementRdbHeader::State=0] Error Report number:16\n"
            "[errcode=17] [elementRdbHeader::State=0] Error Report number:17\n[errcode=18] [elementRdbHeader::State=0] Error Report number:18\n"
            "[errcode=19] [elementRdbHeader::State=0] Error Report number:19\n[errcode=20] [elementRdbHeader::State=0] Error Report number:20\n"
            "[errcode=21] [elementRdbHeader::State=0] Error Report number:21\n[errcode=22] [elementRdbHeader::State=0] Error Report number:22\n"
            "[errcode=23] [elementRdbHeader::State=0] Error Report number:23\n[errcode=24] [elementRdbHeader::State=0] Error Report number:24\n"
            "[errcode=25] [elementRdbHeader::State=0] Error Report number:25\n[errcode=26] [elementRdbHeader::State=0] Error Report number:26\n"
            "[errcode=27] [el\n... last recorded error message: ...\n[errcode=999] [elementRdbHeader::State=0] Error Report number:999\n";

    assert_string_equal(returned, expected);
    assert_int_equal(RDB_getErrorCode(parser), 1001);
    RDB_deleteParser(parser);
}

static void printResPicture(int result) {
    if (result)
        printf("    x_x\n"
               "    /|\\\n"
               "    / \\\n"
               "Tests got failed!\n\n");
    else
        printf("    \\o/\n"
               "     |\n"
               "    / \\\n"
               "All tests passed!!!\n\n");

}


#define RUN_TEST_GROUP(grp) \
    if ((runGroupPrefix == NULL) || (strncmp(runGroupPrefix, #grp, strlen(runGroupPrefix)) == 0)) { \
        printf ("\n--- Test Group: %s ---\n", #grp); \
        result |= grp(); \
    }

/*************************** group_examples ***************************
 * Test the examples in the './examples' directory. These examples
 * do not necessarily support asynchronous events 'WAIT_MORE_DATA.'
 * Therefore, 'group_examples()' should not be called when the environment
 * variable 'LIBRDB_SIM_WAIT_MORE_DATA' is set to '1'.
 *********************************************************************/
int group_examples(void) {
    /* Insert here your test functions */
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_examples),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}

/*************************** group_misc *******************************/
int group_misc(void) {
    /* Insert here your test functions */
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_createReader_missingFile),
        cmocka_unit_test(test_empty_rdb),
        cmocka_unit_test(test_mixed_levels_registration),
        cmocka_unit_test(test_checksum),
        cmocka_unit_test(__test_opcode_ram_lru),
        cmocka_unit_test(test_report_long_error),
        cmocka_unit_test(test_not_support_future_rdb_version),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}

/*************************** MAIN *******************************/
int main(int argc, char *argv[]) {
    struct timeval st, et;
    char *testFilter = NULL, *runGroupPrefix = NULL;
    int result = 0;

    char *redisInstallFolder = getenv("LIBRDB_REDIS_FOLDER");

    const char *USAGE = "Usage: <cmd> [OPTIONS]\n"
                        "Options:\n"
                        "  -h, --help                       Show this help message\n"
                        "  -f, --redis-folder <folder>      Specify the Redis folder to use for the tests\n"
                        "  -g, --test-group <group-prefix>  Selected test group to run\n"
                        "  -t, --test <filter>              Selected test to run\n"
                        "  -v, --valgrind                   Run tests under valgrind";



    /* Parse command-line arguments */
    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)) {
            printf("%s\n", USAGE);
            exit(EXIT_SUCCESS);
        } else if ((strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--redis-folder") == 0) && i+1 < argc) {
            redisInstallFolder = argv[++i];
        } else if ((strcmp(argv[i], "-g") == 0 || strcmp(argv[i], "--test-group") == 0) && i+1 < argc) {
            runGroupPrefix = argv[++i];
        } else if ((strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--test") == 0) && i+1 < argc) {
            testFilter = argv[++i];
        } else if ((strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--valgrind") == 0)) {
            setValgrind();
        } else {
            printf("Invalid argument: %s\n%s\n", argv[i], USAGE);
            exit(EXIT_FAILURE);
        }
    }

    if (testFilter) cmocka_set_test_filter(testFilter);

    gettimeofday(&st,NULL);

    cleanTmpFolder();

    /* Setup redis if configured */
    setRedisInstallFolder(redisInstallFolder);
    setupRedisServer(NULL);

    //setenv("LIBRDB_DEBUG_DATA", "1", 1); /* << to see parser states printouts */

    printf("\n*************** START TESTING *******************\n");
    setEnvVar("LIBRDB_SIM_WAIT_MORE_DATA", "0");
    RUN_TEST_GROUP(group_examples);
    RUN_TEST_GROUP(group_test_resp_reader);
    RUN_TEST_GROUP(group_rdb_to_resp);
    RUN_TEST_GROUP(group_misc);
    RUN_TEST_GROUP(group_rdb_to_json);
    RUN_TEST_GROUP(group_mem_management);
    RUN_TEST_GROUP(group_bulk_ops);
    RUN_TEST_GROUP(group_pause);
    RUN_TEST_GROUP(group_rdb_to_redis); /*external*/
    RUN_TEST_GROUP(group_test_rdb_cli); /*external*/

    printf("\n*************** SIMULATING WAIT_MORE_DATA *******************\n");
    setEnvVar("LIBRDB_SIM_WAIT_MORE_DATA", "1");
    RUN_TEST_GROUP(group_misc);
    RUN_TEST_GROUP(group_rdb_to_resp);
    RUN_TEST_GROUP(group_rdb_to_json);
    RUN_TEST_GROUP(group_mem_management);
    RUN_TEST_GROUP(group_bulk_ops);
    RUN_TEST_GROUP(group_rdb_to_redis); /*external*/
    RUN_TEST_GROUP(group_test_rdb_cli); /*external*/

    printf("\n*************** END TESTING *******************\n");

    gettimeofday(&et, NULL);

    int elapsed = (et.tv_sec - st.tv_sec) * 1000 + (et.tv_usec - st.tv_usec) / 1000;
    printf("Total time: %d milliseconds\n", elapsed);

    /* teardown redis if configured */
    teardownRedisServer();

    cleanup_json_sign_service();

    printResPicture(result);

    return result;
}
