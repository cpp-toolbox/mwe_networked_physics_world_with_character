#include "client.hpp"
#include "game_loop/game_loop.hpp"
#include "input_snapshot/input_snapshot.hpp"
#include "window/window.hpp"
#include <thread>

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

    ClientNetwork client_network(&input_snapshot);
    client_network.attempt_to_connect_to_server();

    std::thread input_sending_thread(&ClientNetwork::start_input_sending_loop, std::ref(client_network));
    input_sending_thread.detach();

    std::thread game_state_receive_thread(&ClientNetwork::start_game_state_receive_loop, std::ref(client_network));
    game_state_receive_thread.detach();

    game_loop.start(60, update, render, termination);
}
