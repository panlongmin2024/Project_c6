#include <sdk.h>
#include <greatest_api.h>

/* Declare a local suite. */
SUITE(other_suite);

TEST blah(void) {
    TEST_PASS();
}

TEST todo(void) {
    TEST_SKIPm("TODO");
}

ADD_TEST_CASE("other_suite") {
    RUN_TEST(blah);
    RUN_TEST(todo);
}
