#include <enet.h>
#include <stdio.h>
#include <string>

unsigned int binary_input_snapshot_to(unsigned int binary_input_snapshot) {
    // bool l = this->input_snapshot->left_pressed;
    // bool r = this->input_snapshot->right_pressed;
    // bool f = this->input_snapshot->forward_pressed;
    // bool b = this->input_snapshot->backward_pressed;
    // bool j = this->input_snapshot->jump_pressed;

    // bool inputs[5] = {l, r, f, b, j};
    //
    // unsigned int binary_input_snapshot = 0;
    //
    std::string temp[5] = {"l", "r", "f", "b", "j"};

    for (int i = 0; i < 5; i++) {
        if (binary_input_snapshot & (1 << i)) {
            std::string message = temp[i] + " pressed\n";
            printf("%s", message.c_str());
        }
    }
    return binary_input_snapshot;
}

int run_server_loop() {

    if (enet_initialize() != 0) {
        printf("An error occurred while initializing ENet.\n");
        return 1;
    }

    ENetAddress address = {0};

    address.host = ENET_HOST_ANY; /* Bind the server to the default localhost. */
    address.port = 7777;          /* Bind the server to port 7777. */

    int MAX_CLIENTS = 32;

    /* create a server */
    ENetHost *server = enet_host_create(&address, MAX_CLIENTS, 2, 0, 0);

    if (server == NULL) {
        printf("An error occurred while trying to create an ENet server host.\n");
        return 1;
    }

    printf("Started a server...\n");

    ENetEvent event;

    int wait_time_seconds = 10;
    int wait_time_milliseconds = wait_time_seconds * 1000;

    /* Wait for an event. (WARNING: blocking) */
    while (enet_host_service(server, &event, wait_time_milliseconds) > 0) {
        switch (event.type) {
        case ENET_EVENT_TYPE_CONNECT:
            printf("A new client connected from %x:%u.\n", event.peer->address.host, event.peer->address.port);
            /* Store any relevant client information here. */
            event.peer->data = (void *)"Client information";
            break;

        case ENET_EVENT_TYPE_RECEIVE: {
            printf("A packet of length %lu containing %s was received from %s on "
                   "channel %u.\n",
                   event.packet->dataLength, event.packet->data, event.peer->data, event.channelID);
            // not everything is going to be an input snapshot, but it works for now.
            bool packet_is_input_snapshot = true;
            if (packet_is_input_snapshot) {
                unsigned int binary_input_snapshot = *event.packet->data;
                binary_input_snapshot_to(binary_input_snapshot);
            }
            /* Clean up the packet now that we're done using it. */
            enet_packet_destroy(event.packet);
        } break;

        case ENET_EVENT_TYPE_DISCONNECT:
            printf("%s disconnected.\n", event.peer->data);
            /* Reset the peer's client information. */
            event.peer->data = NULL;
            break;

        case ENET_EVENT_TYPE_DISCONNECT_TIMEOUT:
            printf("%s disconnected due to timeout.\n", event.peer->data);
            /* Reset the peer's client information. */
            event.peer->data = NULL;
            break;

        case ENET_EVENT_TYPE_NONE:
            break;
        }
    }

    enet_host_destroy(server);
    enet_deinitialize();
    return 0;
}
