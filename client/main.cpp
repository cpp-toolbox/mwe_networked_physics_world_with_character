#include "game_loop/game_loop.hpp"
#include "input_snapshot/input_snapshot.hpp"
#include "window/window.hpp"

std::function<void()> render_closure(GLFWwindow *window) {
    return [window]() {
        glfwSwapBuffers(window);
        glfwPollEvents();
    };
}

void update(double time_since_last_update) {}

std::function<int()> termination_closure(GLFWwindow *window) {
    return [window]() { return glfwWindowShouldClose(window); };
}

int main() {
    unsigned int window_width_px = 600, window_height_px = 600;
    bool start_in_fullscreen = false;
    InputSnapshot input_snapshot;
    GameLoop game_loop;

    GLFWwindow *window = initialize_glfw_glad_and_return_window(&window_width_px, &window_height_px, "client",
                                                                start_in_fullscreen, &input_snapshot);

    std::function<int()> termination = termination_closure(window);
    std::function<void()> render = render_closure(window);

    game_loop.start(60, update, render, termination);
}
