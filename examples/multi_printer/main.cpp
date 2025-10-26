#include "multi_printer.hpp"

#include <iostream>

// Example classes to use with MultiPrinter
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
    std::cout << "=== MultiPrinter with 3 types ===\n";
    MultiPrinter<ClassA, ClassB, ClassC> printer_3;

    printer_3.print<ClassA>(); // Should print 1
    printer_3.print<ClassB>(); // Should print 2
    printer_3.print<ClassC>(); // Should print 3

    std::cout << "\n=== MultiPrinter with 5 types ===\n";
    MultiPrinter<ClassA, ClassB, ClassC, ClassD, ClassE> printer_5;

    printer_5.print<ClassA>(); // Should print 1
    printer_5.print<ClassB>(); // Should print 2
    printer_5.print<ClassC>(); // Should print 3
    printer_5.print<ClassD>(); // Should print 4
    printer_5.print<ClassE>(); // Should print 5

    std::cout << "\n=== Using print with instances ===\n";
    ClassA instance_a;
    ClassB instance_b;
    ClassC instance_c;

    printer_3.print(instance_a); // Should print 1
    printer_3.print(instance_b); // Should print 2
    printer_3.print(instance_c); // Should print 3

    std::cout << "\n=== Using print_at_index ===\n";
    printer_3.print_at_index(1); // Should call ClassA::print()
    printer_3.print_at_index(2); // Should call ClassB::print()
    printer_3.print_at_index(3); // Should call ClassC::print()

    std::cout << "\n=== Using print_at_index with 5 types ===\n";
    printer_5.print_at_index(1); // Should call ClassA::print()
    printer_5.print_at_index(2); // Should call ClassB::print()
    printer_5.print_at_index(3); // Should call ClassC::print()
    printer_5.print_at_index(4); // Should call ClassD::print()
    printer_5.print_at_index(5); // Should call ClassE::print()

    return 0;
}
