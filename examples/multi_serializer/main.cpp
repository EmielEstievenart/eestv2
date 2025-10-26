#include "multi_serializer.hpp"

#include <iostream>

// Example classes to use with MultiSerializer
class ClassA
{
public:
    static void print() { std::cout << "ClassA print function\n"; }
};
class ClassB
{
public:
    static void print() { std::cout << "ClassB print function\n"; }
};
class ClassC
{
public:
    static void print() { std::cout << "ClassC print function\n"; }
};
class ClassD
{
public:
    static void print() { std::cout << "ClassD print function\n"; }
};
class ClassE
{
public:
    static void print() { std::cout << "ClassE print function\n"; }
};

int main()
{
    std::cout << "=== MultiSerializer with 3 types ===\n";
    MultiSerializer<ClassA, ClassB, ClassC> serializer_3;

    serializer_3.print<ClassA>(); // Should print 1
    serializer_3.print<ClassB>(); // Should print 2
    serializer_3.print<ClassC>(); // Should print 3

    std::cout << "\n=== MultiSerializer with 5 types ===\n";
    MultiSerializer<ClassA, ClassB, ClassC, ClassD, ClassE> serializer_5;

    serializer_5.print<ClassA>(); // Should print 1
    serializer_5.print<ClassB>(); // Should print 2
    serializer_5.print<ClassC>(); // Should print 3
    serializer_5.print<ClassD>(); // Should print 4
    serializer_5.print<ClassE>(); // Should print 5

    std::cout << "\n=== Using print with instances ===\n";
    ClassA instance_a;
    ClassB instance_b;
    ClassC instance_c;

    serializer_3.print(instance_a); // Should print 1
    serializer_3.print(instance_b); // Should print 2
    serializer_3.print(instance_c); // Should print 3

    std::cout << "\n=== Using print_at_index ===\n";
    serializer_3.print_at_index(1); // Should call ClassA::print()
    serializer_3.print_at_index(2); // Should call ClassB::print()
    serializer_3.print_at_index(3); // Should call ClassC::print()

    std::cout << "\n=== Using print_at_index with 5 types ===\n";
    serializer_5.print_at_index(1); // Should call ClassA::print()
    serializer_5.print_at_index(2); // Should call ClassB::print()
    serializer_5.print_at_index(3); // Should call ClassC::print()
    serializer_5.print_at_index(4); // Should call ClassD::print()
    serializer_5.print_at_index(5); // Should call ClassE::print()

    return 0;
}
