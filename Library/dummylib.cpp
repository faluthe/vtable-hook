#include "dummylib.hpp"
#include <stdio.h>

int TestClass::TestMethod(int x, int y)
{
    printf("x: %d, y: %d\n", x, y);

    return x + y;
}

void TestClass::TestMethod2()
{
    printf("TestMethod2\n");
}

void TestClass::TestMethod3()
{
    printf("TestMethod3\n");
}

bool TestClass::TestMethod4()
{
    printf("TestMethod4\n");

    return true;
}

ITestInterface *CreateTestClass()
{
    return new TestClass();
}