#include <stdio.h>
#include <string.h>

#include <errno.h>

#include <sys/_endian.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>

#include "../../lib/c/lt_arena.h"
#include "../../lib/c/lt_base.h"
#include "../../lib/c/lmp.h"
#include "../../lib/c/liblmp.h"

#define CONFIG_PATH "ss.conf"

s8 populate_scheduler(mem_arena* arena, u8** table) {
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

    populate_scheduler(arena, table);

    time_t action;
    for (;;) {
        time_t timestamp = time(NULL);
        struct tm* time_info = localtime(&timestamp);
        printf("\r=== %02d:%02d:%02d ===", time_info->tm_hour, time_info->tm_min, time_info->tm_sec);
        fflush(stdout);
        if (table[24*time_info->tm_hour + 60*time_info->tm_min + 60*time_info->tm_sec] != NULL && action != timestamp) {
            action = timestamp;

            int socketFd = socket(AF_INET, SOCK_STREAM, 0);
            if (socketFd == -1) {
                lmp_log_print("ss", "Failed to create socket", LMP_PRINT_TYPE_ERROR);
                return 1;
            }

            int opt = 1;
            int s = setsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
            if (s == -1) {
                lmp_log_print("ss", "Failed to set socket option", LMP_PRINT_TYPE_ERROR);
                close(socketFd);
                return 1;
            }

            struct sockaddr_in serverAddr = {0};
            serverAddr.sin_family = AF_INET;
            serverAddr.sin_port = htons(ADMIRAL_PORT_ADMIRAL);
            serverAddr.sin_addr.s_addr = inet_addr(ADMIRAL_HOST_ADMIRAL);

            int c = connect(socketFd, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
            if (c == -1) {
                lmp_log_print("ss", strerror(errno), LMP_PRINT_TYPE_ERROR);
                close(socketFd);
                continue;
            }

            lmp_packet sendPacket = {0};
            lmp_result result = {0};
            lmp_packet_init(&sendPacket);
            lmp_result_init(&result);

            sendPacket.version = 0x02;
            sendPacket.type = LMP_TYPE_SEND;
            sendPacket.arg = LMP_ARG_SEND;
            sendPacket.payload = table[24*time_info->tm_hour + 60*time_info->tm_min + 60*time_info->tm_sec];
            sendPacket.payload_length = LMP_PACKET_PAYLOAD_MAX_SIZE;

            lmp_net_send_packet(socketFd, &sendPacket, &result);

            if (result.error != LMP_ERR_NONE) {
                lmp_log_print("ss", "Failed to serialize and send packet to admiral", LMP_PRINT_TYPE_ERROR);
            }

            close(s);
        }
    }

    return 0;
}
