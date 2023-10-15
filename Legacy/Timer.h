#ifndef TIMER_H
#define TIMER_H

#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>

// Source: https://github.com/99x/timercpp/blob/master/timercpp.h

class Timer {
    std::atomic<bool> _active{ true };
    std::atomic<long> _iteration{ 0 };

public:
    template<typename Function>
    void setTimeout(Function function, int delay);

    template<typename Function>
    void setInterval(Function function, int interval);

    long iteration() { return _iteration.load(); }

    void stop()
    {
        _iteration = 0;
        _active = false;
    }

};

template<typename Function>
void Timer::setTimeout(Function function, int delay) {
    _active = true;
    std::thread t([=]() {
        if (!_active.load()) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        if (!_active.load()) return;
        function();
    });
    t.detach();
}

template<typename Function>
void Timer::setInterval(Function function, int interval) {
    _active = true;
    std::thread t([=]() {
        while (_active.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(interval));
            if (!_active.load()) return;
            function();
            _iteration.fetch_add(1);
        }
    });
    t.detach();
}


#endif // !TIMER_H

