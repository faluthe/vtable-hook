
struct ITestInterface
{
    virtual int TestMethod(int x, int y) = 0;
    virtual void TestMethod2() = 0;
    virtual void TestMethod3() = 0;
};

struct TestClass : ITestInterface
{
    virtual int TestMethod(int x, int y) override;
    virtual void TestMethod2() override;
    virtual void TestMethod3() override;

    int m_x;
    int m_y;

    bool TestMethod4();

    TestClass()
    {
        m_x = 0x2024;
        m_y = 0x1337;
    }
};

extern "C" ITestInterface *CreateTestClass();