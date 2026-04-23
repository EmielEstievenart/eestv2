#pragma once

#include "can_manager.hpp"
#include "can_notify.hpp"

class Container
{
public:
    Container(CanManager& can_manager, int value);

private:
    void handle_engine_signal(CanSignal signal, int value);

    CanManager& _can_manager;
    int _value;
    CanNotify _notify;
};
