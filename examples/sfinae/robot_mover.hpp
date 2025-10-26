#include <type_traits>
#include <utility>
#include <iostream>

namespace eestv
{

// Trait: has a member function `move()` callable on an lvalue T
template <typename, typename = void>
struct has_member_move : std::false_type
{
};

template <typename T>
struct has_member_move<T, std::void_t<decltype(std::declval<T&>().move())> > : std::true_type
{
};

// Trait: has a free function `move(T&)` found by ADL (unqualified call)
template <typename, typename = void>
struct has_adl_move : std::false_type
{
};

template <typename T>
struct has_adl_move<T, std::void_t<decltype(move(std::declval<T&>()))> > : std::true_type
{
};

// Helper to produce better static_assert messages
template <typename T>
struct always_false : std::false_type
{
};

//ADL = Argument Dependant Lookup
// Main dispatcher: prefer member, otherwise ADL free function, otherwise static assert
template <typename RobotType>
void move_the_robot(RobotType& robot)
{
    if constexpr (has_member_move<RobotType>::value)
    {
        // Member exists: call it
        robot.move();
    }
    else if constexpr (has_adl_move<RobotType>::value)
    {
        // No member, but ADL-visible free function exists: unqualified call enables ADL
        move(robot);
    }
    else
    {
        static_assert(always_false<RobotType>::value, "move_the_robot: no member move() or ADL move(T&) found for RobotType");
    }
}

}
