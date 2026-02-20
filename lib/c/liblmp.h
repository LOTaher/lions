/*  liblmp.h - Utilities for the LIONS Middleware Protocol
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


// ===============================================================
// This library contains all and everything LMP
// ===============================================================

#ifndef LIBLMP_H
#define LIBLMP_H
#include <stddef.h>
#include <pthread.h>
#include "lt_base.h"
#include "lmp.h"
#define LT_ARENA_IMPLEMENTATION
#include "lt_arena.h"

// ===============================================================
// Net
// ===============================================================

// TODO(laith): this about making these two static helpers within the lib c file to prevent extrernal linkage
lmp_error lmp_net_send_packet(u32 fd, const lmp_packet* packet, lmp_result* result);
lmp_error lmp_net_recv_packet(u32 fd, u8* buffer, size_t size, lmp_packet* packet, lmp_result* result);
char* lmp_net_get_client(u32 fd, mem_arena* arena);
lmp_error lmp_net_send_packet_to_admiral(char* endpoint, const lmp_packet* packet, lmp_result* result);

// ===============================================================
// Log
// ===============================================================

#define LMP_LOG_COLOR_INFO "\x1b[34m"  // Blue
#define LMP_LOG_COLOR_WARN "\x1b[33m"  // Yellow
#define LMP_LOG_COLOR_ERROR "\x1b[31m"  // Red
#define LMP_LOG_COLOR_RESET "\x1b[0m"

typedef enum {
    LMP_PRINT_TYPE_INFO,
    LMP_PRINT_TYPE_WARN,
    LMP_PRINT_TYPE_ERROR
} lmp_log_print_type;

void lmp_log_print(const char* service, const char* message, lmp_log_print_type type);

// ===============================================================
// Admiral
// ===============================================================

#define ADMIRAL_BACKLOG 15
#define ADMIRAL_QUEUE_CAPACITY 50
#define ADMIRAL_QUEUE_READ_RETRY_SECONDS 30

#define ADMIRAL_PORT_ADMIRAL 5321
#define ADMIRAL_HOST_ADMIRAL "100.109.120.90" // inferno
#define ADMIRAL_ENDPOINT_ADMIRAL "100.109.120.90:5321"

#define ADMIRAL_PORT_HOTEL 4200
#define ADMIRAL_HOST_HOTEL "100.103.121.7" // nuke
#define ADMIRAL_ENDPOINT_HOTEL "100.103.121.7:4200"

#define ADMIRAL_PORT_SCHEDULER 6767
#define ADMIRAL_HOST_SCHEDULER "100.103.121.7" // nuke
#define ADMIRAL_ENDPOINT_SCHEDULER "100.103.121.7:6767"

typedef struct {
    u8 destinationId;
    u8 senderId;
    lmp_packet packet;
} lmp_admiral_message;

typedef struct {
    char* name;
    u8 id;
} lmp_admiral_message_endpoint_metadata;

typedef struct {
    mem_arena* arena;
    lmp_admiral_message** messages;
    u8 size;
    u8 capacity;
    u8 head;
    u8 tail;
    pthread_mutex_t mutex;
} lmp_admiral_queue;

// NOTE(laith): this will be updated to add all the services that admiral will support
// put new endpoints in between hotel and scheduler, also add it to lmp_admiral_map_client_to_endpoint
typedef enum {
    ADMIRAL,
    HOTEL,
    SCHEDULER
} lmp_admiral_endpoint;

typedef struct {
    lmp_admiral_queue* queue;
} lmp_admiral_network_args;

typedef struct {
    lmp_admiral_queue* queue;
} lmp_admiral_admiral_args;

void lmp_admiral_queue_init(lmp_admiral_queue* queue, u8 capacity);
s8 lmp_admiral_queue_enqueue(lmp_admiral_queue* queue, const lmp_admiral_message* message);
lmp_admiral_message* lmp_admiral_queue_dequeue(lmp_admiral_queue* queue);

s8 lmp_admiral_add_packet_to_queue(lmp_admiral_queue* queue, lmp_packet* packet, char* endpoint);
void lmp_admiral_invalidate_packet(lmp_packet* packet);
void lmp_admiral_sanitize_message(lmp_admiral_message* message);

char* lmp_admiral_map_client_to_endpoint(char* client);
char* lmp_admiral_map_id_to_endpoint(u8 id);

#endif // LIBLMP_H
