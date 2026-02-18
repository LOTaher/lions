#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../include/lt_arena.h"
#include "../include/lt_base.h"
#include "../include/lmp.h"
#include "../include/liblmp.h"

#define CONFIG_PATH "ss.conf"

s8 populate_schedule(mem_arena* arena, u8** table) {
    FILE* f = fopen(CONFIG_PATH, "r");

    if (f == NULL) {
        lmp_log_print("ss", "Error opening config file.", LMP_PRINT_TYPE_ERROR);
        return -1;
    }

    int hour, minute, second;
    u8 payload[LMP_PACKET_PAYLOAD_MAX_SIZE] = {0};
    char logBuffer[255];

    for (;;) {
        memset(logBuffer, 0, sizeof(logBuffer));

        int s = fscanf(f, "%d:%d:%d %s", &hour, &minute, &second, payload);
        if (s == -1) {
            break;
        }


        u8* allocatedPayload = arena_push(arena, LMP_PACKET_PAYLOAD_MAX_SIZE);
        memcpy(allocatedPayload, payload, LMP_PACKET_PAYLOAD_MAX_SIZE);
        u32 destination = (hour * 24) + (minute * 60) + (second * 60);

        snprintf(logBuffer, sizeof(logBuffer), "Scheduled job for %02d:%02d:%02d", hour, minute, second);

        table[destination] = allocatedPayload;

        lmp_log_print("ss", logBuffer, LMP_PRINT_TYPE_INFO);
    }

    fclose(f);
    return 1;
}

int main(void) {
    mem_arena* arena = arena_create(KiB(10));
    u8* table[86400] = {0};


    populate_schedule(arena, table);

    // u8* data = arena_push(arena, sizeof(lmp_packet));

    time_t action;
    for (;;) {
        time_t timestamp = time(NULL);
        struct tm* time_info = localtime(&timestamp);
        printf("\r=== %02d:%02d:%02d ===", time_info->tm_hour, time_info->tm_min, time_info->tm_sec);
        fflush(stdout);
        if (table[24*time_info->tm_hour + 60*time_info->tm_min + 60*time_info->tm_sec] != NULL && action != timestamp) {
            action = timestamp;
            printf("PERFORM ACTION\n");
        }
    }

    return 0;
}
