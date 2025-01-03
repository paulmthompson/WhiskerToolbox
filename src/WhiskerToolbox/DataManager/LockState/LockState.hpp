#ifndef LOCKSTATE_HPP
#define LOCKSTATE_HPP

#include <unordered_map>
#include <vector>

class LockState {
public:
    LockState() = default;

    void lock(int time) {
        _lock_states[time] = true;
    }

    void unlock(int time) {
        _lock_states[time] = false;
    }

    bool isLocked(int time) const {
        auto it = _lock_states.find(time);
        if (it != _lock_states.end()) {
            return it->second;
        }
        return false;
    }

    std::vector<int> getLockedTimes() const {
        std::vector<int> locked_times;
        for (const auto& [time, locked] : _lock_states) {
            if (locked) {
                locked_times.push_back(time);
            }
        }
        return locked_times;
    }

private:
    std::unordered_map<int, bool> _lock_states;
};

#endif // LOCKSTATE_HPP
