#ifndef RATE_LIMITED_LOOP_HPP
#define RATE_LIMITED_LOOP_HPP

#include <functional>
#include "../stopwatch/stopwatch.hpp"

class RateLimitedLoop {
  public:
    void start(double update_rate_hz, const std::function<void(double)> &rate_limited_func,
               const std::function<bool()> &termination_condition_func);
    Stopwatch loop_stopwatch;
};

#endif // RATE_LIMITED_LOOP_HPP
