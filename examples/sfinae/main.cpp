#include <iostream>

#include "walking_robot.hpp"
#include "make_robot_do_something.hpp"
#include "flying_robot.hpp"
#include "robot_mover.hpp"
#include "decltype_declval_string.hpp"
// struct IRobot
// {
// };

// struct WalkRobot : IRobot
// {
//     static const char WALKABLE {};
//     void Walk() { std::cout << "Robot is walking\n"; }
// };

// struct FlyRobot : IRobot
// {
//     static const char FLY {};
//     void Fly() { std::cout << "Robot is flying\n"; }
// };

// template <class T> void DoSomething(T& r, decltype(T::WALKABLE)* = nullptr)
// {
//     std::cout << "Walkable version called\n";
//     r.Walk();
// }

// template <class T> void DoSomething(T& r, decltype(T::FLY)* = nullptr)
// {
//     std::cout << "Fly version called\n";
//     r.Fly();
// }

void move(eestv::FlyingRobot& robot)
{
    std::cout << "Moving the flying robot from main\n";
}

int main()
{
    // WalkRobot w;
    // DoSomething<WalkRobot>(w);
    // FlyRobot f;
    // DoSomething(f);

    eestv::WalkingRobot walker;
    eestv::do_something<eestv::WalkingRobot>(walker);

    eestv::FlyingRobot flyer;
    eestv::do_something(flyer);

    move_the_robot(walker);

    eestv::do_magic_addition();

    return 0;
}

// // --- Examples ---

// struct WithMember {
//     void move() { std::cout << "WithMember::move()\n"; }
// };

// struct WithFree {};

// // free function in the same (global) namespace as WithFree, discovered by ADL
// void move(WithFree& r) { std::cout << "free move(WithFree&)\n"; }

// struct Neither {};

// int main()
// {
//     WithMember a;
//     WithFree b;
//     Neither c;

//     move_the_robot(a); // calls member
//     move_the_robot(b); // calls free function via ADL
//     // move_the_robot(c); // compile-time error with static_assert
// }
