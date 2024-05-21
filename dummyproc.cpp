#include <stdio.h>
#include <stdlib.h>

#include "dummylib/dummylib.hpp"

int main()
{
    ITestInterface *test = CreateTestClass();
    auto dstruct = new DummyStruct{1, 2};

    test->TestMethod();

    printf("test->TestMethod2(1, 2): %d\n", test->TestMethod2(1, 2));

    printf("test->TestHookMe(1.0f, {1, 2}): %s\n", test->TestHookMe(1.0f, dstruct) ? "true" : "false");

    test->TestMethod3();

    while (true)
    {
        printf("Press any key to run TestHookMe again...\n");
        getchar();

        printf("test->TestHookMe(1.0f, {1, 2}): %s\n", test->TestHookMe(1.0f, dstruct) ? "true" : "false");
    }

    delete test;
    delete dstruct;

    return 0;
}