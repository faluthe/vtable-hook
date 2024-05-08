#include "dummylib.hpp"

#include <stdio.h>

void TestClass::TestMethod()
{
    printf("TestClass::TestMethod()\n");
}

int TestClass::TestMethod2(int x, int y)
{
    printf("TestClass::TestMethod2(%d, %d)\n", x, y);
    return x + y;
}

bool TestClass::TestHookMe(float sampleTime, DummyStruct *dstruct)
{
    printf("TestClass::TestHookMe(%f, {%d, %d})\n", sampleTime, dstruct->x, dstruct->y);
    return false;
}

void TestClass::TestMethod3()
{
    printf("TestClass::TestMethod3()\n");
}

ITestInterface *CreateTestClass()
{
    return new TestClass();
}