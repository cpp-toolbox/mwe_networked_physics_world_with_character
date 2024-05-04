#ifndef MWE_NETWORKING_SERVER_HPP
#define MWE_NETWORKING_SERVER_HPP

#include "enet.h"
#include "input_snapshot/input_snapshot.hpp"
#include "interaction/physics/physics.hpp"

class ServerNetwork {
  public:
    ServerNetwork();
    unsigned int port = 7777;
    ENetHost *server;
    int start_receive_loop(InputSnapshot *input_snapshot);
    std::function<void(double)> game_state_send_step_closure(Physics *physics);
};

#endif // MWE_NETWORKING_SERVER_HPP
