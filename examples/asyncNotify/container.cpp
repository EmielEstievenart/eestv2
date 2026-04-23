#include "container.hpp"

#include <iostream>
#include <magic_enum/magic_enum.hpp>

Container::Container(CanManager& can_manager, int value) :
    _can_manager(can_manager),
    _value(value),
    _notify(
        {CanSignal::engine_running},
        [this](CanSignal signal, int signal_value)
        {
            handle_engine_signal(signal, signal_value);
        })
{
    _can_manager.add_notify(_notify);
}

void Container::handle_engine_signal(CanSignal signal, int value)
{
    _value += value;
    std::cout << "Container " << _value << " received " << magic_enum::enum_name(signal) << " = " << value << '\n';
}
