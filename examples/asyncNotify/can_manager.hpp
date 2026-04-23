#pragma once

#include "can_signals.hpp"

#include <array>
#include <cstddef>
#include <mutex>
#include <vector>

class CanNotify;

class CanManager
{
public:
    CanManager();
    ~CanManager();

    void add_notify(CanNotify& notify);
    void remove_notify(CanNotify& notify);
    void update_signal_value(CanSignal signal, int value);

private:
    static constexpr auto signal_count = static_cast<std::size_t>(CanSignal::count);

    std::array<int, signal_count> _signal_values {};
    std::vector<std::vector<CanNotify*>> _notifies;
    std::mutex _mutex;
};
