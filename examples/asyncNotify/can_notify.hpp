#pragma once

#include "can_signals.hpp"

#include <functional>
#include <vector>

class CanManager;

class CanNotify
{
public:
    using Callback = std::function<void(CanSignal, int)>;

    CanNotify(std::vector<CanSignal> signal_ids, Callback callback);
    ~CanNotify();

    CanNotify(const CanNotify&)            = delete;
    CanNotify& operator=(const CanNotify&) = delete;

    const std::vector<CanSignal>& signal_ids() const;
    void notify(CanSignal signal, int value) const;

private:
    friend class CanManager;

    std::vector<CanSignal> _signal_ids;
    Callback _callback;
    CanManager* _manager {nullptr};
};
