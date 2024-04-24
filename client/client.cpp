#include <stdio.h>
#include "enet.h"
#include "client.hpp"

int connect_and_disconnect() {

    if (enet_initialize() != 0) {
        fprintf(stderr, "An error occurred while initializing ENet.\n");
        return EXIT_FAILURE;
    }

    ENetHost *client = {0};
    client = enet_host_create(NULL /* create a client host */, 1 /* only allow 1 outgoing connection */,
                              2 /* allow up 2 channels to be used, 0 and 1 */,
                              0 /* assume any amount of incoming bandwidth */,
                              0 /* assume any amount of outgoing bandwidth */);
    if (client == NULL) {
        fprintf(stderr, "An error occurred while trying to create an ENet client host.\n");
        exit(EXIT_FAILURE);
    }

    ENetAddress address = {0};
    ENetEvent event = {static_cast<ENetEventType>(0)};
    ENetPeer *peer = {0};
    /* Connect to some.server.net:1234. */
    enet_address_set_host(&address, "127.0.0.1");
    address.port = 7777;
    /* Initiate the connection, allocating the two channels 0 and 1. */
    peer = enet_host_connect(client, &address, 2, 0);
    if (peer == NULL) {
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
        enet_peer_reset(peer);
        puts("Connection to some.server.net:1234 failed.");
    }

    /* Create a reliable packet of size 7 containing "packet\0" */
    ENetPacket *packet = enet_packet_create("packet", strlen("packet") + 1, ENET_PACKET_FLAG_RELIABLE);

    // /* Extend the packet so and append the string "foo", so it now */
    // /* contains "packetfoo\0"                                      */
    // enet_packet_resize(packet, strlen("packetfoo") + 1);
    // strcpy(&packet->data[strlen("packet")], "foo");

    /* Send the packet to the peer over channel id 0. */
    /* One could also broadcast the packet by         */
    /* enet_host_broadcast (host, 0, packet);         */
    enet_peer_send(peer, 0, packet);

    /* One could just use enet_host_service() instead. */
    enet_host_flush(client);

    // Receive some events
    enet_host_service(client, &event, 5000);

    // Disconnect
    enet_peer_disconnect(peer, 0);

    uint8_t disconnected = false;
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
        enet_peer_reset(peer);
    }

    enet_host_destroy(client);
    enet_deinitialize();

    return 0;
}
