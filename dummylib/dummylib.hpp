struct DummyStruct
{
    int x;
    int y;
};

class ITestInterface
{
public:
    virtual ~ITestInterface() {}
    virtual void TestMethod() = 0;
    virtual int TestMethod2(int x, int y) = 0;
    virtual bool TestHookMe(float sampleTime, DummyStruct *dstruct) = 0;
    virtual void TestMethod3() = 0;
};

class TestClass : public ITestInterface
{
public:
    int x;
    float y;
    const char *z;

    TestClass() : x(0), y(0.0f), z("Hello, world!") {}
    virtual ~TestClass() {}
    virtual void TestMethod();
    virtual int TestMethod2(int x, int y);
    virtual bool TestHookMe(float sampleTime, DummyStruct *dstruct);
    virtual void TestMethod3();
};

extern "C" ITestInterface *CreateTestClass();