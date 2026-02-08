// ===============================================================
// This library contains all and everything STMP
// ===============================================================

#ifndef LIBSTMP_H
#define LIBSTMP_H
#include <stddef.h>
#include <pthread.h>
#include "lt_base.h"
#include "stmp.h"
#define LT_ARENA_IMPLEMENTATION
#include "lt_arena.h"

// ===============================================================
// Net
// ===============================================================

stmp_error stmp_net_send_packet(u32 fd, const stmp_packet* packet, stmp_result* result);
stmp_error stmp_net_recv_packet(u32 fd, u8* buffer, size_t size, stmp_packet* packet, stmp_result* result);

char* stmp_net_get_client(u32 fd, mem_arena* arena);

// ===============================================================
// Log
// ===============================================================

#define STMP_LOG_COLOR_INFO "\x1b[34m"  // Blue
#define STMP_LOG_COLOR_WARN "\x1b[33m"  // Yellow
#define STMP_LOG_COLOR_ERROR "\x1b[31m"  // Red
#define STMP_LOG_COLOR_RESET "\x1b[0m"

typedef enum {
    STMP_PRINT_TYPE_INFO,
    STMP_PRINT_TYPE_WARN,
    STMP_PRINT_TYPE_ERROR
} stmp_log_print_type;

void stmp_log_print(const char* service, const char* message, stmp_log_print_type type);

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
    u8 destination;
    u8 sender;
    stmp_packet packet;
} stmp_admiral_message;

typedef struct {
    char* destination;
    char* sender;
} stmp_admiral_message_endpoint_names;

typedef struct {
    mem_arena* arena;
    stmp_admiral_message** messages;
    u8 size;
    u8 capacity;
    u8 head;
    u8 tail;
    pthread_mutex_t mutex;
} stmp_admiral_queue;

// NOTE(laith): this will be updated to add all the services that admiral will support
// put new endpoints in between hotel and scheduler, also add it to stmp_admiral_map_client_to_endpoint
typedef enum {
    ADMIRAL,
    HOTEL,
    SCHEDULER
} stmp_admiral_endpoint;

typedef struct {
    stmp_admiral_queue* queue;
} stmp_admiral_network_args;

typedef struct {
    stmp_admiral_queue* queue;
} stmp_admiral_admiral_args;

void stmp_admiral_queue_init(stmp_admiral_queue* queue, u8 capacity);
s8 stmp_admiral_queue_enqueue(stmp_admiral_queue* queue, const stmp_admiral_message* message);
stmp_admiral_message* stmp_admiral_queue_dequeue(stmp_admiral_queue* queue);
s8 stmp_admiral_parse_and_queue_packet(stmp_admiral_queue* queue, stmp_packet* packet, char* endpoint);
void stmp_admiral_invalidate_packet(stmp_packet* packet);
stmp_admiral_message_endpoint_names stmp_admiral_get_endpoint(stmp_admiral_message* message);
void stmp_admiral_sanitize_message(stmp_admiral_message* message);
char* stmp_admiral_map_client_to_endpoint(char* client);

#endif // LIBSTMP_H
