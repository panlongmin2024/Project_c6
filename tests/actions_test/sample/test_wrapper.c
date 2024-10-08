/********************************************************************************
 *                            USDK(ATS350B_linux)
 *                            Module: SYSTEM
 *                 Copyright(c) 2003-2017 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>      <time>                      <version >          <desc>
 *      wuyufan   2018-8-2-上午9:54:58             1.0             build this file
 ********************************************************************************/
/*!
 * \file     test_wrapper.c
 * \brief
 * \author
 * \version  1.0
 * \date  2018-8-2-上午9:54:58
 *******************************************************************************/
#include <sdk.h>
#include <greatest_api.h>

/* Define a suite, compiled seperately. */
SUITE_EXTERN(other_suite);

/* Declare a local suite. */
SUITE(suite);

enum foo_t { FOO_1, FOO_2, FOO_3 };
static greatest_enum_str_fun foo_str;

/* Just test against random ints, to show a variety of results. */
TEST example_test_case(void) {
    int r = 0;
    TEST_ASSERT(1 == 1);

    r = rand() % 10;
    if (r == 1) TEST_SKIP();
    TEST_ASSERT(r >= 1);
    TEST_PASS();
}

TEST expect_equal(void) {
    int i = 9;
    TEST_ASSERT_EQ(10, i);
    TEST_PASS();
}

TEST expect_str_equal(void) {
    const char *foo1 = "foo1";
    TEST_ASSERT_STR_EQ("foo2", foo1);
    TEST_PASS();
}

TEST expect_strn_equal(void) {
    const char *foo1 = "foo1";
    TEST_ASSERT_STRN_EQ("foo2", foo1, 3);
    TEST_PASS();
}

/* A boxed int type, used to show type-specific equality tests. */
typedef struct {
    int i;
} boxed_int;

/* Callback used to check whether two boxed_ints are equal. */
static int boxed_int_equal_cb(const void *exp, const void *got, void *udata) {
    const boxed_int *ei = (const boxed_int *)exp;
    const boxed_int *gi = (const boxed_int *)got;

    /* udata is not used here, but could be used to specify a comparison
     * resolution, a string encoding, or any other state that should be
     * passed along to the equal and print callbacks. */
    (void)udata;
    return (ei->i == gi->i);
}

/* Callback to print a boxed_int, used to produce an
 * "Exected X, got Y" failure message. */
static int boxed_int_printf_cb(const void *t, void *udata) {
    const boxed_int *bi = (const boxed_int *)t;
    (void)udata;
    return printf("{%d}", bi->i);
}

/* The struct that stores the previous two functions' pointers. */
static greatest_type_info boxed_int_type_info = {
    boxed_int_equal_cb,
    boxed_int_printf_cb,
};

TEST expect_boxed_int_equal(void) {
    boxed_int a = {3};
    boxed_int b = {3};
    boxed_int c = {4};
    TEST_ASSERT_EQUAL_T(&a, &b, &boxed_int_type_info, NULL);  /* succeeds */
    TEST_ASSERT_EQUAL_T(&a, &c, &boxed_int_type_info, NULL);  /* fails */
    TEST_PASS();
}

/* The struct that stores the previous two functions' pointers. */
static greatest_type_info boxed_int_type_info_no_print = {
    boxed_int_equal_cb,
    NULL,
};

TEST expect_boxed_int_equal_no_print(void) {
    boxed_int a = {3};
    boxed_int b = {3};
    boxed_int c = {4};
    (void)boxed_int_printf_cb;
    /* succeeds */
    TEST_ASSERT_EQUAL_T(&a, &b, &boxed_int_type_info_no_print, NULL);
    /* fails */
    TEST_ASSERT_EQUAL_T(&a, &c, &boxed_int_type_info_no_print, NULL);
    TEST_PASS();
}

TEST expect_int_equal_printing_hex(void) {
    unsigned int a = 0xba5eba11;
    unsigned int b = 0xf005ba11;
    TEST_ASSERT_EQ_FMT(a, b, "0x%08x");
    TEST_PASS();
}

TEST expect_floating_point_range(void) {
    TEST_ASSERT_IN_RANGEm("in range",    -0.00001, -0.000110, 0.00010);
    TEST_ASSERT_IN_RANGEm("in range",     0.00001,  0.000110, 0.00010);
    TEST_ASSERT_IN_RANGE(0.00001,  0.000110, 0.00010);
    TEST_ASSERT_IN_RANGEm("out of range", 0.00001,  0.000111, 0.00010);
    TEST_PASS();
}

/* Flag, used to confirm that teardown hook is being called. */
static int teardown_was_called = 0;

TEST teardown_example_TEST_PASS(void) {
    teardown_was_called = 0;
    TEST_PASS();
}

TEST teardown_example_FAIL(void) {
    teardown_was_called = 0;
    TEST_FAILm("Using FAIL to trigger teardown callback");
}

TEST teardown_example_SKIP(void) {
    teardown_was_called = 0;
    TEST_SKIPm("Using SKIP to trigger teardown callback");
}

/* Example of a test case that calls another function which uses TEST_ASSERT. */
static greatest_test_res less_than_three(int arg) {
    TEST_ASSERT(arg <3);
    TEST_PASS();
}

TEST example_using_subfunctions(void) {
    TEST_CHECK_CALL(less_than_three(1)); /* <3 */
    TEST_CHECK_CALL(less_than_three(5)); /* </3 */
    TEST_PASS();
}

/* Example of an ANSI C compatible way to do test cases with
 * arguments: they are passed one argument, a pointer which
 * should be cast back to a struct with the other data. */
TEST parametric_example_c89(void *closure) {
    int arg = *(int *) closure;
    TEST_ASSERT(arg > 10);
    TEST_PASS();
}

/* If using C99, greatest can also do parametric tests without
 * needing to manually manage a closure. */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 19901L
TEST parametric_example_c99(int arg) {
    TEST_ASSERT(arg > 10);
    TEST_PASS();
}
#endif

#if GREATEST_USE_LONGJMP
static greatest_test_res subfunction_with_FAIL_WITH_LONGJMP(int arg) {
    if (arg == 0) {
        FAIL_WITH_LONGJMPm("zero argument (expected failure)");
    }
    TEST_PASS();
}

static greatest_test_res subfunction_with_ASSERT_OR_LONGJMP(int arg) {
    ASSERT_OR_LONGJMPm("zero argument (expected failure)", arg != 0);
    TEST_PASS();
}

TEST fail_via_FAIL_WITH_LONGJMP(void) {
    subfunction_with_FAIL_WITH_LONGJMP(0);
    TEST_PASS();
}

TEST fail_via_FAIL_WITH_LONGJMP_if_0(int arg) {
    subfunction_with_FAIL_WITH_LONGJMP(arg);
    TEST_PASS();
}

TEST fail_via_ASSERT_OR_LONGJMP(void) {
    subfunction_with_ASSERT_OR_LONGJMP(0);
    TEST_PASS();
}
#endif

TEST expect_mem_equal(void) {
    char got[56];
    char exp[sizeof(got)];
    size_t i = 0;
    for (i = 0; i < sizeof(got); i++) {
        exp[i] = (char)i;
        got[i] = (char)i;
    }

    /* Two bytes differ */
    got[23] = 'X';
    got[34] = 'X';

    TEST_ASSERT_MEM_EQm("expected matching memory", got, exp, sizeof(got));
    TEST_PASS();
}

static const char *foo_str(int v) {
    switch ((enum foo_t)v) {
    case FOO_1: return "FOO_1";
    case FOO_2: return "FOO_2";
    case FOO_3: return "FOO_3";
    }
    return "unknown";
}

static int side_effect = 0;

static enum foo_t foo_2_with_side_effect(void) {
    side_effect++;
    return FOO_2;
}

TEST expect_enum_equal(void) {
    TEST_ASSERT_ENUM_EQ(FOO_1, foo_2_with_side_effect(), foo_str);
    TEST_PASS();
}

TEST expect_enum_equal_only_evaluates_args_once(void) {
    /* If the failure case for TEST_ASSERT_ENUM_EQ evaluates GOT more
     * than once, `side_effect` will be != 1 here. */
    TEST_ASSERT_EQ_FMTm("TEST_ASSERT_ENUM_EQ should only evaluate arguments once",
        1, side_effect, "%d");
    TEST_PASS();
}

static size_t Fibonacci(unsigned char x) {
    if (x < 2) {
        return 1;
    } else {
        return Fibonacci(x - 1) + Fibonacci(x - 2);
    }
}

TEST extra_slow_test(void) {
    unsigned char i;
    printf("\nThis test can be skipped with a negative test filter...\n");
    for (i = 1; i < 30; i++) {
        printf("fib %u -> %lu\n", i, (long unsigned)Fibonacci(i));
    }
    TEST_PASS();
}

static void trace_setup(void *arg) {
    printf("-- in setup callback\n");
    teardown_was_called = 0;
    (void)arg;
}

static void trace_teardown(void *arg) {
    printf("-- in teardown callback\n");
    teardown_was_called = 1;
    (void)arg;
}

/* Primary test suite. */
ADD_TEST_CASE("suite") {
    volatile int i = 0;
    int arg = 0;
    printf("\nThis should have some failures:\n");
    for (i=0; i<200; i++) {
        RUN_TEST(example_test_case);
    }
    RUN_TEST(expect_equal);
    printf("\nThis should fail:\n");
    RUN_TEST(expect_str_equal);
    printf("\nThis should pass:\n");
    RUN_TEST(expect_strn_equal);
    printf("\nThis should fail:\n");
    RUN_TEST(expect_boxed_int_equal);
    printf("\nThis should fail:\n");
    RUN_TEST(expect_boxed_int_equal_no_print);

    printf("\nThis should fail, printing the mismatched values in hex.\n");
    RUN_TEST(expect_int_equal_printing_hex);

    printf("\nThis should fail and show floating point values just outside the range.\n");
    RUN_TEST(expect_floating_point_range);

    /* Set so asserts below won't fail if running in list-only or
     * first-fail modes. (setup() won't be called and clear it.) */
    teardown_was_called = -1;

    /* Add setup/teardown for each test case. */
    GREATEST_SET_SETUP_CB(trace_setup, NULL);
    GREATEST_SET_TEARDOWN_CB(trace_teardown, NULL);

    /* Check that the test-specific teardown hook is called. */
    RUN_TEST(teardown_example_TEST_PASS);
    assert(teardown_was_called);

    printf("\nThis should fail:\n");
    RUN_TEST(teardown_example_FAIL);
    assert(teardown_was_called);

    printf("This should be skipped:\n");
    RUN_TEST(teardown_example_SKIP);
    assert(teardown_was_called);

    /* clear setup and teardown */
    GREATEST_SET_SETUP_CB(NULL, NULL);
    GREATEST_SET_TEARDOWN_CB(NULL, NULL);

    printf("This should fail, but note the subfunction that failed.\n");
    RUN_TEST(example_using_subfunctions);

    /* Run a test with one void* argument (which can point to a
     * struct with multiple arguments). */
    printf("\nThis should fail:\n");
    arg = 10;
    RUN_TEST1(parametric_example_c89, &arg);
    arg = 11;
    RUN_TEST1(parametric_example_c89, &arg);

    /* Run a test, with arguments. ('p' for "parametric".) */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 19901L
    printf("\nThis should fail:\n");
    RUN_TESTp(parametric_example_c99, 10);
    RUN_TESTp(parametric_example_c99, 11);
#endif

#if GREATEST_USE_LONGJMP
    RUN_TEST(fail_via_FAIL_WITH_LONGJMP);
    RUN_TEST1(fail_via_FAIL_WITH_LONGJMP_if_0, 0);
    RUN_TEST(fail_via_ASSERT_OR_LONGJMP);
#endif

#if GREATEST_USE_LONGJMP &&                                     \
    (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 19901L)
    RUN_TESTp(fail_via_FAIL_WITH_LONGJMP_if_0, 0);
#endif

    if (GREATEST_IS_VERBOSE()) {
        printf("greatest was run with verbosity level: %u\n",
            greatest_get_verbosity());
    }

    printf("\nThis should fail:\n");
    RUN_TEST(expect_mem_equal);

    printf("\nThis should fail:\n");
    RUN_TEST(expect_enum_equal);

    printf("\nThis should NOT fail:\n");
    RUN_TEST(expect_enum_equal_only_evaluates_args_once);

    RUN_TEST(extra_slow_test);
}
