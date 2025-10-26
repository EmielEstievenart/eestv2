#pragma once

#include <utility>
#include <iostream>

namespace eestv
{

template <typename RobotType>
auto do_something(RobotType& robot, decltype(std::declval<RobotType>().walk())* = nullptr) -> void
{
    std::cout << "Walkable overload \n";
    robot.walk();
}

template <typename RobotType>
auto do_something(RobotType& robot, decltype(std::declval<RobotType>().fly())* = nullptr) -> void
{
    std::cout << "Flyable overload \n";
    robot.fly();
}

/*
We add the i as a parameter because other there is ambiguity with the other do_something functions. 
*/
template <typename RobotType>
auto do_something(RobotType& robot, int i)
{
    std::cout << "Doing whatever \n";
}

}