#include <iostream>

template <typename T>
T add(T arg)
{
    return arg;
}

template <typename T, typename... ARGS>
T add(T first, ARGS... args)
{
    return first + add(args...);
}

int main()
{
    std::cout << add(1, 2, 3, 4, 5);
    return 0;
}
