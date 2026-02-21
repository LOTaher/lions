/*  liblmp.c - Utilities for the LIONS Middleware Protocol
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


#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <errno.h>

#include "liblmp.h"
#include "lt_arena.h"
#include "lt_base.h"
#include "lmp.h"

// ===============================================================
// Net
// ===============================================================

lmp_error lmp_net_send_packet(u32 fd, const lmp_packet* packet, lmp_result* result) {
    u8 buffer[LMP_PACKET_MAX_SIZE];
    lmp_packet_serialize(buffer, sizeof(buffer), packet, result);
    if (result->error != LMP_ERR_NONE) {
        return result->error;
    }

    ssize_t sent = send(fd, buffer, result->size, 0);
    if (sent < 0) {
        return LMP_ERR_BAD_INPUT;
    }

    return result->error;
}

lmp_error lmp_net_recv_packet(u32 fd, u8* buffer, size_t size, lmp_packet* packet, lmp_result* result) {
    u8 terminated = 0;
    u8 invalid = 0;
    u64 packet_size = 0;

    for (;;) {
        u8 scratch[LMP_PACKET_MAX_SIZE];

        int bytes = recv(fd, scratch, sizeof(scratch), 0);
        if (bytes <= 0) {
            break;
        }

        for (int i = 0; i < bytes; i++) {
            buffer[packet_size] = scratch[i];
            packet_size++;

            if (packet_size >= size) {
                invalid = 1;
                break;
            }

            if (scratch[i] == LMP_PACKET_TERMINATE) {
                terminated = 1;
                break;
            }
        }

        if (terminated || invalid) break;
    }

    if (terminated) {
        lmp_packet_deserialize(buffer, packet_size, packet, result);
        return result->error;
    }

    if (invalid) {
        return LMP_ERR_BAD_SIZE;
    }

    return LMP_ERR_BAD_INPUT;
}

char* lmp_net_get_client(u32 fd, mem_arena* arena) {
    struct sockaddr clientAddr = {0};
    socklen_t clientAddrLen = sizeof(clientAddr);
    int g = getpeername(fd, (struct sockaddr*)&clientAddr, &clientAddrLen);
    if (g == -1) {
        return NULL;
    }

    char host[255] = {0};
    char port[255] = {0};

    int n = getnameinfo(&clientAddr, clientAddrLen, host, sizeof(host), port, sizeof(port), NI_NUMERICHOST | NI_NUMERICSERV);
    if (n != 0) {
        return NULL;
    }

    char name[255] = {0};
    snprintf(name, sizeof(name), "%s:%s", host, port);

    char* allocatedName = arena_push(arena, sizeof(name));
    memcpy(allocatedName, name, sizeof(name));

    return allocatedName;
}


// lmp_error lmp_net_send_packet_to_admiral(char* endpoint, const lmp_packet* packet, lmp_result* result) {
//     mem_arena* arena = arena_create(KiB(8));
//
//     return NULL;
// }

// ===============================================================
// Log
// ===============================================================

const char* lmp_log_print_type_colors[] = {
    LMP_LOG_COLOR_INFO,
    LMP_LOG_COLOR_WARN,
    LMP_LOG_COLOR_ERROR
};

void lmp_log_print(const char* service, const char* message, lmp_log_print_type type) {
	time_t timestamp = time(NULL);
	struct tm* time_info = localtime(&timestamp);

    switch (type) {
        case LMP_PRINT_TYPE_INFO:
            fprintf(stderr, "%s[%s] %02d:%02d:%02d [INFO]: %s%s\n", lmp_log_print_type_colors[type], service, time_info->tm_hour, time_info->tm_min, time_info->tm_sec, message, LMP_LOG_COLOR_RESET);
            return;
        case LMP_PRINT_TYPE_WARN:
            fprintf(stderr, "%s[%s] %02d:%02d:%02d [WARN]: %s%s\n", lmp_log_print_type_colors[type], service, time_info->tm_hour, time_info->tm_min, time_info->tm_sec, message, LMP_LOG_COLOR_RESET);
            return;
        case LMP_PRINT_TYPE_ERROR:
            fprintf(stderr, "%s[%s] %02d:%02d:%02d [ERROR]: %s%s\n", lmp_log_print_type_colors[type], service, time_info->tm_hour, time_info->tm_min, time_info->tm_sec, message, LMP_LOG_COLOR_RESET);
            return;
    }
}

// ===============================================================
// Admiral
// ===============================================================

char* lmp_admiral_endpoints[] = {
    "admiral",
    "hotel",
    "scheduler",
};

void lmp_admiral_queue_init(lmp_admiral_queue* queue, u8 capacity) {
    mem_arena* arena = arena_create(MiB(10));
    queue->arena = arena;
    queue->size = 0;
    queue->capacity = capacity;
    queue->head = 0;
    queue->tail = 0;

    queue->messages = arena_push(queue->arena, sizeof(lmp_admiral_message*) * capacity);

    pthread_mutex_init(&queue->mutex, NULL);
}

s8 lmp_admiral_queue_enqueue(lmp_admiral_queue* queue, const lmp_admiral_message* message) {
    pthread_mutex_lock(&queue->mutex);

    if (queue->size >= queue->capacity) {
        pthread_mutex_unlock(&queue->mutex);
        return -1;
    }

    lmp_admiral_message* allocated = arena_push(queue->arena, sizeof(lmp_admiral_message));

    *allocated = *message;

    queue->messages[queue->tail++] = allocated;
    queue->size++;

    pthread_mutex_unlock(&queue->mutex);

    return 1;
}

lmp_admiral_message* lmp_admiral_queue_dequeue(lmp_admiral_queue* queue) {
    pthread_mutex_lock(&queue->mutex);

    if (queue->size == 0) {
        pthread_mutex_unlock(&queue->mutex);
        return NULL;
    }

    lmp_admiral_message* msg = queue->messages[queue->head++];

    queue->size--;

    if (queue->size == 0) {
        arena_clear(queue->arena);
        queue->head = 0;
        queue->tail = 0;
    }

    pthread_mutex_unlock(&queue->mutex);

    return msg;
}

// NOTE(laith): this function changes how ownership of the packet is handled. This paacket lives
// in the network loop arena and now it is getting copied over to the queue arena. with that, after
// this function ends, we can safely pop the packet memory of the network arena and start again
//
// Do NOT share memory across threads!
s8 lmp_admiral_add_packet_to_queue(lmp_admiral_queue* queue, lmp_packet* packet, char* endpoint) {
    char logBuffer[255] = {0};

    // NOTE(laith): this should be [dest][sender][EMPTY PAYLOAD BYTE] at the minimum
    if (packet->payload_length < 3) {
        return -1;
    }

    // NOTE(laith): this converts and ascii string to its byte form
    u8 destination = packet->payload[0] - '0';
    u8 sender = packet->payload[1] - '0';

    if (destination > SCHEDULER || sender > SCHEDULER) {
        return -1;
    }

    switch (sender) {
        case ADMIRAL:
            if (strcmp(endpoint, "admiral") != 0) {
                snprintf(logBuffer, sizeof(logBuffer), "[%s] is claiming to be a [admiral]", endpoint);
                lmp_log_print("admiral", logBuffer, LMP_PRINT_TYPE_ERROR);
                return -1;
            }

            break;
        case HOTEL:
            if (strcmp(endpoint, "hotel") != 0) {
                snprintf(logBuffer, sizeof(logBuffer), "[%s] is claiming to be a [hotel]", endpoint);
                lmp_log_print("admiral", logBuffer, LMP_PRINT_TYPE_ERROR);
                return -1;
            }

            break;
        case SCHEDULER:
            if (strcmp(endpoint, "scheduler") != 0) {
                snprintf(logBuffer, sizeof(logBuffer), "[%s] is claiming to be a [scheduler]", endpoint);
                lmp_log_print("admiral", logBuffer, LMP_PRINT_TYPE_ERROR);
                return -1;
            }

            break;
    }

    lmp_admiral_message message = {destination, sender, *packet};
    s8 e = lmp_admiral_queue_enqueue(queue, &message);
    if (e == -1) {
        snprintf(logBuffer, sizeof(logBuffer), "Could not enqueue message from [%s]", endpoint);
        lmp_log_print("admiral", logBuffer, LMP_PRINT_TYPE_ERROR);
        return -1;
    }

    snprintf(logBuffer, sizeof(logBuffer), "Recieved and added message from [%s] to queue", endpoint);
    lmp_log_print("admiral", logBuffer, LMP_PRINT_TYPE_INFO);

    return 1;
}

// NOTE(laith): probably want some further checks here but given the checks within the enqueue call
// and the protocol itself, it should be fine?
void lmp_admiral_sanitize_message(lmp_admiral_message* message) {
    u64 sanitizedPayloadSize = message->packet.payload_length - 2;
    u8 sanitizedPayload[sanitizedPayloadSize];

    u64 sanitizedPayloadInput = 0;
    for (u64 i = 2; i < message->packet.payload_length; i++) {
        sanitizedPayload[sanitizedPayloadInput++] = message->packet.payload[i];
    }

    memcpy((void*)message->packet.payload, sanitizedPayload, sanitizedPayloadSize);
    message->packet.payload_length = sanitizedPayloadSize;
}

void lmp_admiral_invalidate_packet(lmp_packet* packet) {
    packet->type = LMP_TYPE_INVALID;
    packet->arg = LMP_ARG_INVALID_PAYLOAD;
    packet->flags = LMP_FLAGS_NONE;
    packet->payload = LMP_PAYLOAD_EMPTY;
    packet->payload_length = 1;
}

char* lmp_admiral_map_client_to_endpoint(char* client) {
    if (strcmp(client, ADMIRAL_ENDPOINT_ADMIRAL) == 0) {
        return "admiral";
    }

    if (strcmp(client, ADMIRAL_ENDPOINT_HOTEL) == 0) {
        return "hotel";
    }

    if (strcmp(client, ADMIRAL_ENDPOINT_SCHEDULER) == 0) {
        return "scheduler";
    }

    return NULL;
}

static char* endpoint[] = {
    "admiral",
    "hotel",
    "scheduler"
};

char* lmp_admiral_map_id_to_endpoint(u8 id) {
    return endpoint[id];
}

