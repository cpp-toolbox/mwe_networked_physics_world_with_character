#include <thread>

#include "server.hpp"

int main() {
    std::thread t(run_server_loop);
    t.join();
}
