/*  admiral.c - The LIONS distributed system message broker
    Copyright (C) 2026 splatte.dev

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>. */


#include <netinet/in.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>

#include "../../lib/c/lt_arena.h"
#include "../../lib/c/lt_base.h"
#include "../../lib/c/lmp.h"
#include "../../lib/c/liblmp.h"

void* network_loop(void* args) {
    lmp_admiral_network_args* a = (lmp_admiral_network_args*)args;

    mem_arena* networkArena = arena_create(KiB(8));

    char logBuffer[255];

    int socketFd = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFd == -1) {
        lmp_log_print("admiral", "Failed to create socket", LMP_PRINT_TYPE_ERROR);
        return NULL;
    }

    int opt = 1;
    if (setsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        lmp_log_print("admiral", "Failed to set socket option", LMP_PRINT_TYPE_ERROR);
        close(socketFd);
        return NULL;
    }

    struct sockaddr_in serverAddr = {0};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(ADMIRAL_PORT_ADMIRAL);
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    int b = bind(socketFd, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    if (b == -1) {
        lmp_log_print("admiral", "Failed to bind to socket", LMP_PRINT_TYPE_ERROR);
        close(socketFd);
        return NULL;
    }

    int l = listen(socketFd, ADMIRAL_BACKLOG);
    if (l == -1) {
        lmp_log_print("admiral", "Failed to bind to listen", LMP_PRINT_TYPE_ERROR);
        close(socketFd);
       return NULL;
    }

    snprintf(logBuffer, sizeof(logBuffer), "Listening on %d", ADMIRAL_PORT_ADMIRAL);
    lmp_log_print("admiral", logBuffer, LMP_PRINT_TYPE_INFO);

    for (;;) {
        memset(logBuffer, 0, sizeof(logBuffer));

        struct sockaddr_in clientAddr;
        socklen_t clientLength = sizeof(clientAddr);

        int connectionFd = accept(socketFd, (struct sockaddr *)&clientAddr, &clientLength);
        if (connectionFd == -1) {
            lmp_log_print("admiral", "Failed to accept connection", LMP_PRINT_TYPE_ERROR);
            continue;
        }

        char* client = lmp_net_get_client(connectionFd, networkArena);

        if (client == NULL) {
            lmp_log_print("admiral", "Could not parse client information", LMP_PRINT_TYPE_ERROR);
            continue;
        }

        char* endpoint = lmp_admiral_map_client_to_endpoint(client);
        if (endpoint == NULL) {
            lmp_log_print("admiral", "Bad client connected", LMP_PRINT_TYPE_ERROR);
            continue;
        }

        snprintf(logBuffer, sizeof(logBuffer), "Accepted connection from [%s]", endpoint);
        lmp_log_print("admiral", logBuffer, LMP_PRINT_TYPE_INFO);

        lmp_packet* readPacket = arena_push(networkArena, sizeof(lmp_packet));
        lmp_packet sendPacket;
        lmp_result result;

        lmp_packet_init(readPacket);
        lmp_packet_init(&sendPacket);
        lmp_result_init(&result);
        u8 buffer[LMP_PACKET_MAX_SIZE];

        lmp_error error = lmp_net_recv_packet(connectionFd, buffer, sizeof(buffer), readPacket, &result);
        if (error != LMP_ERR_NONE) {
            close(connectionFd);
            memset(logBuffer, 0, sizeof(logBuffer));
            snprintf(logBuffer, sizeof(logBuffer), "Recieved bad packet from [%s]. Closing connection", endpoint);
            lmp_log_print("admiral", logBuffer, LMP_PRINT_TYPE_ERROR);
            arena_clear(networkArena);
            continue;
        }

        s8 p = lmp_admiral_add_packet_to_queue(a->queue, readPacket, endpoint);
        if (p == -1) {
            lmp_admiral_invalidate_packet(&sendPacket);
            lmp_error send_error = lmp_net_send_packet(connectionFd, &sendPacket, &result);

            if (send_error != LMP_ERR_NONE) {
                lmp_log_print("admiral", "Could not send invalid response.", LMP_PRINT_TYPE_WARN);
            }

            close(connectionFd);
            memset(logBuffer, 0, sizeof(logBuffer));
            snprintf(logBuffer, sizeof(logBuffer), "Recieved invalid admiral packet from [%s]. Closing connection", endpoint);
            lmp_log_print("admiral", logBuffer, LMP_PRINT_TYPE_ERROR);
            arena_clear(networkArena);
            continue;
        }

        arena_clear(networkArena);
        close(connectionFd);
    }

    arena_destroy(networkArena);
    close(socketFd);
    return 0;
}

void* admiral_loop(void* args) {
    lmp_admiral_admiral_args* a = (lmp_admiral_admiral_args*)args;

    char logBuffer[255];

    for (;;) {
        memset(logBuffer, 0, sizeof(logBuffer));
        lmp_admiral_message* msg = lmp_admiral_queue_dequeue(a->queue);

        if (msg == NULL) {
            snprintf(logBuffer, sizeof(logBuffer),"No message in the queue. Retrying in %d seconds", ADMIRAL_QUEUE_READ_RETRY_SECONDS);
            lmp_log_print("admiral", logBuffer, LMP_PRINT_TYPE_WARN);
            sleep(ADMIRAL_QUEUE_READ_RETRY_SECONDS);
            continue;
        }

        char* destinationName = lmp_admiral_map_id_to_endpoint(msg->destinationId);
        char* senderName = lmp_admiral_map_id_to_endpoint(msg->senderId);

        lmp_admiral_sanitize_message(msg);

        snprintf(logBuffer, sizeof(logBuffer), "Forwarding message to [%s] from [%s]",
                 destinationName, senderName);

        lmp_log_print("admiral", logBuffer, LMP_PRINT_TYPE_INFO);
    }

    // TODO(laith): send the net packet, for now lets log to test
    // use the new endpoint macros to connect to the IP and port to send

    return 0;
}

int main(void) {
    lmp_admiral_queue queue;
    lmp_admiral_queue_init(&queue, ADMIRAL_QUEUE_CAPACITY);

    lmp_admiral_network_args networkArgs = {
        .queue = &queue,
    };

    pthread_t networkThread;

    pthread_create(&networkThread, NULL, network_loop, (void*)&networkArgs);

    lmp_admiral_admiral_args admiralArgs = {
        .queue = &queue,
    };

    pthread_t admiralThread;

    pthread_create(&admiralThread, NULL, admiral_loop, (void*)&admiralArgs);

    pthread_join(networkThread, NULL);
    pthread_join(admiralThread, NULL);

    return 0;
}

