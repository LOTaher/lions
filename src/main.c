#include <netinet/in.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>

#include "../include/lt_arena.h"
#include "../include/lt_base.h"
#include "../include/stmp.h"
#include "../include/libstmp.h"

void* network_loop(void* args) {
    stmp_admiral_network_args* a = (stmp_admiral_network_args*)args;

    mem_arena* networkArena = arena_create(KiB(8));

    char logBuffer[255];

    int socketFd = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFd == -1) {
        stmp_log_print("admiral", "Failed to create socket", STMP_PRINT_TYPE_ERROR);
        return NULL;
    }

    int opt = 1;
    if (setsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        stmp_log_print("admiral", "Failed to set socket option", STMP_PRINT_TYPE_ERROR);
        close(socketFd);
        return NULL;
    }

    struct sockaddr_in serverAddr = {0};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(ADMIRAL_PORT_ADMIRAL);
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    int b = bind(socketFd, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    if (b == -1) {
        stmp_log_print("admiral", "Failed to bind to socket", STMP_PRINT_TYPE_ERROR);
        close(socketFd);
        return NULL;
    }

    int l = listen(socketFd, ADMIRAL_BACKLOG);
    if (l == -1) {
        stmp_log_print("admiral", "Failed to bind to listen", STMP_PRINT_TYPE_ERROR);
        close(socketFd);
       return NULL;
    }

    snprintf(logBuffer, sizeof(logBuffer), "Listening on %d", ADMIRAL_PORT_ADMIRAL);
    stmp_log_print("admiral", logBuffer, STMP_PRINT_TYPE_INFO);

    for (;;) {
        memset(logBuffer, 0, sizeof(logBuffer));
        stmp_log_print("admiral", "Waiting for connection", STMP_PRINT_TYPE_INFO);

        struct sockaddr_in clientAddr;
        socklen_t clientLength = sizeof(clientAddr);

        int connectionFd = accept(socketFd, (struct sockaddr *)&clientAddr, &clientLength);
        if (connectionFd == -1) {
            stmp_log_print("admiral", "Failed to accept connection", STMP_PRINT_TYPE_ERROR);
            continue;
        }

        char* client = stmp_net_get_client(connectionFd, networkArena);

        if (client == NULL) {
            stmp_log_print("admiral", "Could not parse client information", STMP_PRINT_TYPE_ERROR);
            continue;
        }

        char* endpoint = stmp_admiral_map_client_to_endpoint(client);
        if (endpoint == NULL) {
            stmp_log_print("admiral", "Bad client connected", STMP_PRINT_TYPE_ERROR);
            continue;
        }

        snprintf(logBuffer, sizeof(logBuffer), "Accepted connection from [%s]", endpoint);
        stmp_log_print("admiral", logBuffer, STMP_PRINT_TYPE_INFO);

        u64 mark = arena_mark(networkArena);
        stmp_packet* readPacket = arena_push(networkArena, sizeof(stmp_packet));
        stmp_packet sendPacket;
        stmp_result result;

        stmp_packet_init(readPacket);
        stmp_packet_init(&sendPacket);
        stmp_result_init(&result);
        u8 buffer[STMP_PACKET_MAX_SIZE];

        stmp_error error = stmp_net_recv_packet(connectionFd, buffer, sizeof(buffer), readPacket, &result);
        if (error != STMP_ERR_NONE) {
            arena_pop(networkArena, mark);
            close(connectionFd);
            memset(logBuffer, 0, sizeof(logBuffer));
            snprintf(logBuffer, sizeof(logBuffer), "Recieved bad packet from [%s]. Closing connection", endpoint);
            stmp_log_print("admiral", logBuffer, STMP_PRINT_TYPE_ERROR);
            continue;
        }

        s8 p = stmp_admiral_parse_and_queue_packet(a->queue, readPacket, endpoint);
        if (p == -1) {
            arena_pop(networkArena, mark);
            stmp_admiral_invalidate_packet(&sendPacket);
            stmp_error send_error = stmp_net_send_packet(connectionFd, &sendPacket, &result);

            if (send_error != STMP_ERR_NONE) {
                stmp_log_print("admiral", "Could not send invalid response.", STMP_PRINT_TYPE_WARN);
            }

            close(connectionFd);
            memset(logBuffer, 0, sizeof(logBuffer));
            snprintf(logBuffer, sizeof(logBuffer), "Recieved invalid admiral packet from [%s]. Closing connection", endpoint);
            stmp_log_print("admiral", logBuffer, STMP_PRINT_TYPE_ERROR);
            continue;
        }

        memset(logBuffer, 0, sizeof(logBuffer));
        snprintf(logBuffer, sizeof(logBuffer), "Message from [%s] queued. Closing connection", endpoint);
        stmp_log_print("admiral", logBuffer, STMP_PRINT_TYPE_INFO);
        arena_pop(networkArena, mark);
        close(connectionFd);
    }

    arena_destroy(networkArena);
    close(socketFd);
    return 0;
}

void* admiral_loop(void* args) {
    stmp_admiral_admiral_args* a = (stmp_admiral_admiral_args*)args;

    char logBuffer[255];

    for (;;) {
        memset(logBuffer, 0, sizeof(logBuffer));
        stmp_admiral_message* msg = stmp_admiral_queue_dequeue(a->queue);

        if (msg == NULL) {
            snprintf(logBuffer, sizeof(logBuffer),"No message in the queue. Retrying in %d seconds", ADMIRAL_QUEUE_READ_RETRY_SECONDS);
            stmp_log_print("admiral", logBuffer, STMP_PRINT_TYPE_WARN);
            sleep(ADMIRAL_QUEUE_READ_RETRY_SECONDS);
            continue;
        }

        // TODO(laith): within this struct, add destinationIP, destinationPort, etc. can rename
        // to stmp_admiral_get_endpoint_metadata
        stmp_admiral_message_endpoint_names endpoints = stmp_admiral_get_endpoint(msg);
        stmp_admiral_sanitize_message(msg);

        snprintf(logBuffer, sizeof(logBuffer), "Forwarding message to [%s] from [%s]",
                 endpoints.destination, endpoints.sender);

        stmp_log_print("admiral", logBuffer, STMP_PRINT_TYPE_INFO);
    }

    // TODO(laith): send the net packet, for now lets log to test
    // this would consist of mapping the endpoint ids to a port and IP

    return 0;
}

int main(void) {
    stmp_admiral_queue queue;
    stmp_admiral_queue_init(&queue, ADMIRAL_QUEUE_CAPACITY);

    stmp_admiral_network_args networkArgs = {
        .queue = &queue,
    };

    pthread_t networkThread;

    pthread_create(&networkThread, NULL, network_loop, (void*)&networkArgs);

    stmp_admiral_admiral_args admiralArgs = {
        .queue = &queue,
    };

    pthread_t admiralThread;

    pthread_create(&admiralThread, NULL, admiral_loop, (void*)&admiralArgs);

    pthread_join(networkThread, NULL);
    pthread_join(admiralThread, NULL);

    return 0;
}

