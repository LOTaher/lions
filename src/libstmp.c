#include <stdio.h>
#include <string.h>
#include <sys/_types/_socklen_t.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <pthread.h>

#include "../include/libstmp.h"
#include "../include/lt_arena.h"
#include "../include/lt_base.h"
#include "../include/stmp.h"

// ===============================================================
// Net
// ===============================================================

stmp_error stmp_net_send_packet(u32 fd, const stmp_packet* packet, stmp_result* result) {
    u8 buffer[STMP_PACKET_MAX_SIZE];
    stmp_packet_serialize(buffer, sizeof(buffer), packet, result);
    if (result->error != STMP_ERR_NONE) {
        return result->error;
    }

    ssize_t sent = send(fd, buffer, result->size, 0);
    if (sent < 0) {
        return STMP_ERR_BAD_INPUT;
    }

    return result->error;
}

stmp_error stmp_net_recv_packet(u32 fd, u8* buffer, size_t size, stmp_packet* packet, stmp_result* result) {
    u8 terminated = 0;
    u8 invalid = 0;
    u64 packet_size = 0;

    for (;;) {
        u8 scratch[STMP_PACKET_MAX_SIZE];

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

            if (scratch[i] == STMP_PACKET_TERMINATE) {
                terminated = 1;
                break;
            }
        }

        if (terminated || invalid) break;
    }

    if (terminated) {
        stmp_packet_deserialize(buffer, packet_size, packet, result);
        return result->error;
    }

    if (invalid) {
        return STMP_ERR_BAD_SIZE;
    }

    return STMP_ERR_BAD_INPUT;
}

stmp_net_get_client_values stmp_net_get_client(u32 fd) {

    struct sockaddr clientAddr = {0};
    socklen_t clientAddrLen = sizeof(clientAddr);
    int g = getpeername(fd, &clientAddr, &clientAddrLen);

    if (g == -1) {
        return (stmp_net_get_client_values) {
            .name = NULL,
            .success = -1
        };
    }

    char host[255];
    char server[255];

    int n = getnameinfo(&clientAddr, clientAddrLen, host, sizeof(host), server, sizeof(server), NI_NUMERICHOST | NI_NUMERICSERV);

    if (n != 0) {
        return (stmp_net_get_client_values) {
            .name = NULL,
            .success = -1
        };
    }

    printf("Host: %s | Port: %s\n", host, server);

    return (stmp_net_get_client_values) {
        .name = "test",
        .success = 1
    };
}

// ===============================================================
// Log
// ===============================================================

const char* stmp_log_print_type_colors[] = {
    STMP_LOG_COLOR_INFO,
    STMP_LOG_COLOR_WARN,
    STMP_LOG_COLOR_ERROR
};

void stmp_log_print(const char* service, const char* message, stmp_log_print_type type) {
	time_t timestamp = time(NULL);
	struct tm* time_info = localtime(&timestamp);

    switch (type) {
        case STMP_PRINT_TYPE_INFO:
            fprintf(stderr, "%s[%s] %d:%d:%d [INFO]: %s%s\n", stmp_log_print_type_colors[type], service, time_info->tm_hour, time_info->tm_min, time_info->tm_sec, message, STMP_LOG_COLOR_RESET);
            return;
        case STMP_PRINT_TYPE_WARN:
            fprintf(stderr, "%s[%s] %d:%d:%d [WARN]: %s%s\n", stmp_log_print_type_colors[type], service, time_info->tm_hour, time_info->tm_min, time_info->tm_sec, message, STMP_LOG_COLOR_RESET);
            return;
        case STMP_PRINT_TYPE_ERROR:
            fprintf(stderr, "%s[%s] %d:%d:%d [ERROR]: %s%s\n", stmp_log_print_type_colors[type], service, time_info->tm_hour, time_info->tm_min, time_info->tm_sec, message, STMP_LOG_COLOR_RESET);
            return;
    }
}

// ===============================================================
// Admiral
// ===============================================================

char* stmp_admiral_endpoints[] = {
    "admiral",
    "hotel",
    "scheduler",
};

void stmp_admiral_queue_init(stmp_admiral_queue* queue, u8 capacity) {
    mem_arena* arena = arena_create(MiB(10));
    queue->arena = arena;
    queue->size = 0;
    queue->capacity = capacity;
    queue->head = 0;
    queue->tail = 0;

    queue->messages = arena_push(queue->arena, sizeof(stmp_admiral_message*) * capacity);

    pthread_mutex_init(&queue->mutex, NULL);
}

s8 stmp_admiral_queue_enqueue(stmp_admiral_queue* queue, const stmp_admiral_message* message) {
    pthread_mutex_lock(&queue->mutex);

    if (queue->size >= queue->capacity) {
        pthread_mutex_unlock(&queue->mutex);
        return -1;
    }

    stmp_admiral_message* allocated = arena_push(queue->arena, sizeof(stmp_admiral_message));

    *allocated = *message;

    queue->messages[queue->tail++] = allocated;
    queue->size++;

    pthread_mutex_unlock(&queue->mutex);

    return 1;
}

stmp_admiral_message* stmp_admiral_queue_dequeue(stmp_admiral_queue* queue) {
    pthread_mutex_lock(&queue->mutex);

    if (queue->size == 0) {
        pthread_mutex_unlock(&queue->mutex);
        return NULL;
    }

    stmp_admiral_message* msg = queue->messages[queue->head++];

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
s8 stmp_admiral_parse_and_queue_packet(stmp_admiral_queue* queue, stmp_packet* packet) {
    // NOTE(laith): this should be [dest][sender][EMPTY PAYLOAD BYTE] at the minimum
    if (packet->payload_length < 3) {
        return STMP_ERR_BAD_PAYLOAD;
    }

    u8 destination = packet->payload[0];
    u8 sender = packet->payload[1];

    if ((destination < HOTEL || destination > SCHEDULER) ||
        (sender < HOTEL || destination > SCHEDULER)) {
        return STMP_ERR_BAD_PAYLOAD;
    }

    stmp_admiral_message message = {destination, sender, *packet};
    s8 e = stmp_admiral_queue_enqueue(queue, &message);
    if (e == -1) {
        stmp_log_print("admiral", "Could not enqueue message", STMP_PRINT_TYPE_ERROR);
        return -1;
    }

    stmp_log_print("admiral", "Recieved and added message to queue", STMP_PRINT_TYPE_INFO);

    return 1;
}

// NOTE(laith): probably want some further checks here but given the checks within the enqueue call
// and the protocol itself, it should be fine?
void stmp_admiral_sanitize_message(stmp_admiral_message* message) {
    u64 sanitizedPayloadSize = message->packet.payload_length - 2;
    u8 sanitizedPayload[sanitizedPayloadSize];

    u64 sanitizedPayloadInput = 0;
    for (u64 i = 2; i < message->packet.payload_length; i++) {
        sanitizedPayload[sanitizedPayloadInput++] = message->packet.payload[i];
    }

    memcpy((void*)message->packet.payload, sanitizedPayload, sanitizedPayloadSize);
    message->packet.payload_length = sanitizedPayloadSize;
}

stmp_admiral_message_endpoint_names stmp_admiral_get_endpoint(stmp_admiral_message* message) {
    u8 destination = message->packet.payload[0];
    u8 sender = message->packet.payload[1];

    stmp_admiral_message_endpoint_names names = {
        .destination = stmp_admiral_endpoints[destination],
        .sender = stmp_admiral_endpoints[sender],
    };

    return names;
}

void stmp_admiral_invalidate_packet(stmp_packet* packet) {
    packet->type = STMP_TYPE_INVALID;
    packet->arg = STMP_ARG_INVALID_PAYLOAD;
    packet->flags = STMP_FLAGS_NONE;
    packet->payload = STMP_PAYLOAD_EMPTY;
    packet->payload_length = 1;
}


