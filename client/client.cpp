#include <linux/input.h>
#include <stdexcept>
#include <stdio.h>
#include "client.hpp"
#include "input_snapshot/input_snapshot.hpp"
#include "rate_limited_loop/rate_limited_loop.hpp"

ClientNetwork::ClientNetwork(InputSnapshot *input_snapshot) : input_snapshot(input_snapshot) {
    initialize_client_network();
}
ClientNetwork::~ClientNetwork() { disconnect_from_server(); }

void ClientNetwork::initialize_client_network() {
    if (enet_initialize() != 0) {
        fprintf(stderr, "An error occurred while initializing ENet.\n");
        throw std::runtime_error("An error occurred while initializing ENet.\n");
        // return EXIT_FAILURE;
    }

    client = {0};
    client = enet_host_create(NULL /* create a client host */, 1 /* only allow 1 outgoing connection */,
                              2 /* allow up 2 channels to be used, 0 and 1 */,
                              0 /* assume any amount of incoming bandwidth */,
                              0 /* assume any amount of outgoing bandwidth */);
    if (client == NULL) {
        fprintf(stderr, "An error occurred while trying to create an ENet client host.\n");
        throw std::runtime_error("An error occurred while trying to create an ENet client host.\n");
        exit(EXIT_FAILURE);
    }
}

/**
 * \pre initialize_client_network was run successfully
 */
void ClientNetwork::attempt_to_connect_to_server() {

    ENetAddress address = {0};
    ENetEvent event = {static_cast<ENetEventType>(0)};
    server_connection = {0};
    /* Connect to some.server.net:1234. */
    enet_address_set_host(&address, "127.0.0.1");
    address.port = 7777;
    /* Initiate the connection, allocating the two channels 0 and 1. */
    server_connection = enet_host_connect(client, &address, 2, 0);
    if (server_connection == NULL) {
        fprintf(stderr, "No available peers for initiating an ENet connection.\n");
        exit(EXIT_FAILURE);
    }
    /* Wait up to 5 seconds for the connection attempt to succeed. */
    if (enet_host_service(client, &event, 5000) > 0 && event.type == ENET_EVENT_TYPE_CONNECT) {
        puts("Connection to some.server.net:1234 succeeded.");
    } else {
        /* Either the 5 seconds are up or a disconnect event was */
        /* received. Reset the peer in the event the 5 seconds   */
        /* had run out without any significant event.            */
        enet_peer_reset(server_connection);
        puts("Connection to some.server.net:1234 failed.");
    }
}

void ClientNetwork::start_input_sending_loop() {

    RateLimitedLoop rate_limited_loop;

    ENetEvent event = {static_cast<ENetEventType>(0)};

    std::function<bool()> termination_func = []() { return false; };

    std::function<void()> rate_limited_func = [this]() {
        unsigned int binary_input_snapshot = this->input_snapshot_to_binary();
        printf("%d\n", binary_input_snapshot);
        ENetPacket *packet =
            enet_packet_create(&binary_input_snapshot, sizeof(binary_input_snapshot), ENET_PACKET_FLAG_RELIABLE);

        enet_peer_send(server_connection, 0, packet);

        /* One could just use enet_host_service() instead. */
        enet_host_flush(client);
    };

    rate_limited_loop.start(60, rate_limited_func, termination_func);
}

unsigned int ClientNetwork::input_snapshot_to_binary() {
    bool l = this->input_snapshot->left_pressed;
    bool r = this->input_snapshot->right_pressed;
    bool f = this->input_snapshot->forward_pressed;
    bool b = this->input_snapshot->backward_pressed;
    bool j = this->input_snapshot->jump_pressed;

    bool inputs[5] = {l, r, f, b, j};

    unsigned int binary_input_snapshot = 0;

    for (int i = 0; i < 5; i++) {
        if (inputs[i]) {
            binary_input_snapshot = binary_input_snapshot | 1 << i;
        }
    }
    return binary_input_snapshot;
}

/**
 * \pre you've connected to the server
 */
void ClientNetwork::attempt_to_send_test_packet() {

    ENetEvent event = {static_cast<ENetEventType>(0)};
    /* Create a reliable packet of size 7 containing "packet\0" */
    ENetPacket *packet = enet_packet_create("packet", strlen("packet") + 1, ENET_PACKET_FLAG_RELIABLE);

    // about to put a while loop here

    // /* Extend the packet so and append the string "foo", so it now */
    // /* contains "packetfoo\0"                                      */
    // enet_packet_resize(packet, strlen("packetfoo") + 1);
    // strcpy(&packet->data[strlen("packet")], "foo");

    /* Send the packet to the peer over channel id 0. */
    /* One could also broadcast the packet by         */
    /* enet_host_broadcast (host, 0, packet);         */
    enet_peer_send(server_connection, 0, packet);

    /* One could just use enet_host_service() instead. */
    enet_host_flush(client);

    // Receive some events
    // enet_host_service(client, &event, 5000);
}

int ClientNetwork::start_game_state_receive_loop(glm::vec3 *character_position) {
    ENetEvent event;
    int wait_time_milliseconds = 100;

    while (true) {
        /* Wait for an event. (WARNING: blocking, which is ok, since this is in it's own thread) */
        while (enet_host_service(this->client, &event, wait_time_milliseconds) > 0) {
            switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT: // is this even possible on the client?
                printf("A new client connected from %x:%u.\n", event.peer->address.host, event.peer->address.port);
                /* Store any relevant client information here. */
                event.peer->data = (void *)"Client information";
                break;

            case ENET_EVENT_TYPE_RECEIVE: {
                // printf("A packet of length %lu containing %s was received from %s on "
                //        "channel %u.\n",
                //        event.packet->dataLength, event.packet->data, event.peer->data, event.channelID);

                // not everything is going to be an input snapshot, but it works for now.
                bool packet_is_player_position = true;
                if (packet_is_player_position) {
                    float *player_position = (float *)event.packet->data;
                    printf("position %f %f %f\n", player_position[0], player_position[1], player_position[2]);
                    character_position->x = player_position[0];
                    character_position->y = player_position[1];
                    character_position->z = player_position[2];
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
    }

    enet_host_destroy(this->client);
    enet_deinitialize();
    return 0;
}

void ClientNetwork::disconnect_from_server() {

    // Disconnect
    enet_peer_disconnect(server_connection, 0);

    uint8_t disconnected = false;

    ENetEvent event = {static_cast<ENetEventType>(0)};
    /* Allow up to 3 seconds for the disconnect to succeed
     * and drop any packets received packets.
     */
    while (enet_host_service(client, &event, 3000) > 0) {
        switch (event.type) {
        case ENET_EVENT_TYPE_RECEIVE:
            enet_packet_destroy(event.packet);
            break;
        case ENET_EVENT_TYPE_DISCONNECT:
            puts("Disconnection succeeded.");
            disconnected = true;
            break;
        }
    }

    // Drop connection, since disconnection didn't successed
    if (!disconnected) {
        enet_peer_reset(server_connection);
    }

    enet_host_destroy(client);
    enet_deinitialize();
}
