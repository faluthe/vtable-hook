# What is `virtual`?

Virtual function tables are a result of the `virtual` keyword in C++. When a function in a base class is marked with the `virtual` keyword, derrived classes may override that function. Then, at runtime when the function is called, the address of the function is resolved using the actual runtime object's type. We'll take a closer look at how this all works in a moment. For now we can think of it like this: for a base class with virtual methods there is a list of overwritable methods we can choose to overwrite when we create a derrived class. Say we have a `virtual void printType()` function in our `Shape` base class that prints "I am a Shape" to the console. We could create a new type `Square` that inherits from `Shape` and since `printType` is in the list of virtual methods we can override it and make it print "I am a Square" instead. We can see that in action here:

```cpp
#include <stdio.h>

struct Shape
{
    virtual void printType() { printf("I am a Shape\n"); }
}

struct Square : Shape
{
    virtual void printType() override { printf("I am a Square\n"); }
}

void printShape(Shape& shape)
{
    shape.printType();
}

Square square;
// Should print "I am a Square"
printShape(square);
```

In this example, both `Shape` and `Square` each have vtables that are identical in structure.

# What is an Interface?

Interfaces are pure virtual classes; the class has no data members only member functions and all member functions are pure virtual.

```cpp
struct IMyInterface
{
    virtual bool TestFunc() = 0;
    virtual void AnotherFunc() = 0;
    virtual int LastFunc() = 0;
}
```
A pure virtual function indicates that derrived classes MUST override the function.