#include "rate_limited_loop.hpp"

void RateLimitedLoop::start(double update_rate_hz, const std::function<void(double)> &rate_limited_func,
                            const std::function<bool()> &termination_condition_func) {

    double time_elapsed_since_start_of_program = 0;

    // 1/N seconds per iteration
    double time_between_state_update = 1.0 / update_rate_hz;

    // std::chrono::duration<double> time_elapsed_since_last_state_update;
    double time_elapsed_since_last_state_update = 0;

    bool first_iteration = true;

    std::chrono::time_point<std::chrono::system_clock> time_at_start_of_iteration;
    std::chrono::time_point<std::chrono::system_clock> time_at_start_of_iteration_last_iteration;

    // double time_at_start_of_iteration_last_iteration = -1.0;
    double duration_of_last_iteration = -1.0;

    while (!termination_condition_func()) {

        time_at_start_of_iteration = std::chrono::system_clock::now(); // (T)

        if (first_iteration) {
            // The last few lines of this iteration are next loops last iteration.
            first_iteration = false;
            time_at_start_of_iteration_last_iteration = time_at_start_of_iteration; // (C)
            continue;
        }

        // Note that this measures how long it takes for the code to start at (T) and arrive back at (T),
        // (G): Due to (C) tesli == 0 on the second iteration, and non-zero after that, this doesn't cause any issues.
        std::chrono::duration<double> delta = time_at_start_of_iteration - time_at_start_of_iteration_last_iteration;
        duration_of_last_iteration = delta.count();

        // None of the updates that could have happened during the last iteration have been applied
        // This is because last iteration, we retroactively applied last last iterations updates
        time_elapsed_since_last_state_update += duration_of_last_iteration;

        // since the value of teslsu is only updated by (E), this would always be false, but (F) bootstraps the process
        bool enough_time_for_updates = time_elapsed_since_last_state_update >= time_between_state_update;

        // Due to the (G), an update could only happen starting from the 3rd iteration
        if (enough_time_for_updates) {

            // retroactively apply updates that should have occurred during previous iterations
            double time_remaining_to_fit_updates = time_elapsed_since_last_state_update;
            bool enough_time_to_fit_update = true;

            while (enough_time_to_fit_update) {

                rate_limited_func(time_between_state_update);
                loop_stopwatch.press();

                time_remaining_to_fit_updates -= time_between_state_update;
                enough_time_to_fit_update = time_remaining_to_fit_updates >= time_between_state_update;
            }
            time_elapsed_since_last_state_update = time_remaining_to_fit_updates;
        }

        // With respect to the start of the next iteration, the code down here is previous iteration.
        time_at_start_of_iteration_last_iteration = time_at_start_of_iteration;
    }
}
