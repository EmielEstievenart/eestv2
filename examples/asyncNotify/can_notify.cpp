#include "can_notify.hpp"

#include "can_manager.hpp"

#include <utility>

CanNotify::CanNotify(std::vector<CanSignal> signal_ids, Callback callback) : _signal_ids(std::move(signal_ids)), _callback(std::move(callback))
{
}

CanNotify::~CanNotify()
{
    if (_manager != nullptr)
    {
        _manager->remove_notify(*this);
    }
}

const std::vector<CanSignal>& CanNotify::signal_ids() const
{
    return _signal_ids;
}

void CanNotify::notify(CanSignal signal, int value) const
{
    if (_callback)
    {
        _callback(signal, value);
    }
}
