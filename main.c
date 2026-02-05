#include "lt_arena.h"
#include "lt_base.h"
#include "stmp.h"
#include "libstmp.h"

#include <netinet/in.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/_pthread/_pthread_t.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>

#define ADMIRAL_PORT 5321
#define ADMIRAL_BACKLOG 15
#define ADMIRAL_QUEUE_CAPACITY 50
#define ADMIRAL_QUEUE_READ_RETRY_SECONDS 30

void* network_loop(void* args) {
    stmp_admiral_network_args* a = (stmp_admiral_network_args*)args;

    mem_arena* networkArena = arena_create(KiB(8));

    int socketFd = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFd == -1) {
        stmp_log_print("admiral", "Failed to create socket", ERROR);
        return NULL;
    }

    int opt = 1;
    if (setsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        stmp_log_print("admiral", "Failed to set socket option", ERROR);
        close(socketFd);
        return NULL;
    }

    struct sockaddr_in serverAddr = {0};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(ADMIRAL_PORT);
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    int b = bind(socketFd, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    if (b == -1) {
        stmp_log_print("admiral", "Failed to bind to socket", ERROR);
        close(socketFd);
        return NULL;
    }

    int l = listen(socketFd, ADMIRAL_BACKLOG);
    if (l == -1) {
        stmp_log_print("admiral", "Failed to bind to listen", ERROR);
        close(socketFd);
       return NULL;
    }

    stmp_log_print("admiral", "Listening on specified port...", INFO);

    for (;;) {
        stmp_log_print("admiral", "Waiting for connection...", INFO);

        struct sockaddr_in clientAddr;
        socklen_t clientLength = sizeof(clientAddr);

        int connectionFd = accept(socketFd, (struct sockaddr *)&clientAddr, &clientLength);
        if (connectionFd == -1) {
            stmp_log_print("admiral", "Failed to accept connection", ERROR);
            continue;
        }

        stmp_log_print("admiral", "Accepted connection", INFO);

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
            stmp_log_print("admiral", "Recieved a bad packet. Closing connection.", ERROR);
            continue;
        }

        error = stmp_admiral_parse_and_queue_packet(a->queue, readPacket);
        if (error != STMP_ERR_NONE) {
            arena_pop(networkArena, mark);
            stmp_admiral_invalidate_packet(&sendPacket);
            stmp_error send_error = stmp_net_send_packet(connectionFd, &sendPacket, &result);

            if (send_error != STMP_ERR_NONE) {
                stmp_log_print("admiral", "Could not send invalid response.", ERROR);
            }

            close(connectionFd);
            stmp_log_print("admiral", "Recieved non admiral STMP packet. Closing connection.", WARN);
            continue;
        }

        stmp_log_print("admiral", "Message queued. Closing connection", INFO);
        arena_pop(networkArena, mark);
        close(connectionFd);
    }

    arena_destroy(networkArena);
    close(socketFd);
    return 0;
}


void* admiral_loop(void* args) {
    stmp_admiral_admiral_args* a = (stmp_admiral_admiral_args*)args;
    for (;;) {
        stmp_admiral_message* msg = stmp_admiral_queue_dequeue(a->queue);

        if (msg == NULL) {
            stmp_log_print("admiral", "No message in the queue. Retrying shortly", WARN);
            sleep(ADMIRAL_QUEUE_READ_RETRY_SECONDS);
            continue;
        }

        stmp_admiral_message_endpoint_names endpoints = stmp_admiral_get_endpoint(msg);
        stmp_admiral_sanitize_message(msg);

        char logBuffer[255];
        snprintf(logBuffer, sizeof(logBuffer), "Forwarding message to [%s] from [%s]",
                 endpoints.destination, endpoints.sender);

        stmp_log_print("admiral", logBuffer, INFO);
    }

    // TODO(laith): send the net packet, for now lets log to test

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

