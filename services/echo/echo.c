/*  echo.c - The LIONS distributed system scheduling service
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>

#include "../../lib/c/lt_arena.h"
#include "../../lib/c/lt_base.h"
#include "../../lib/c/lmp.h"
#include "../../lib/c/liblmp.h"

#define CONFIG_PATH "echo.conf"

s8 populate_scheduler(mem_arena* arena, u8** table) {
    FILE* f = fopen(CONFIG_PATH, "r");

    if (f == NULL) {
        lmp_log_print("echo", "Error opening config file.", LMP_PRINT_TYPE_ERROR);
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

        lmp_log_print("echo", logBuffer, LMP_PRINT_TYPE_INFO);
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
                lmp_log_print("echo", "Failed to create socket", LMP_PRINT_TYPE_ERROR);
                return 1;
            }

            int opt = 1;
            int s = setsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
            if (s == -1) {
                lmp_log_print("echo", "Failed to set socket option", LMP_PRINT_TYPE_ERROR);
                close(socketFd);
                return 1;
            }

            struct sockaddr_in localAddr = {0};
            localAddr.sin_family = AF_INET;
            localAddr.sin_port = htons(ADMIRAL_PORT_SCHEDULER);
            localAddr.sin_addr.s_addr = INADDR_ANY;

            int b = bind(socketFd, (struct sockaddr*)&localAddr, sizeof(localAddr));
            if (b == -1) {
                lmp_log_print("echo", "Failed to bind to port", LMP_PRINT_TYPE_ERROR);
                close(socketFd);
                continue;
            }

            struct sockaddr_in serverAddr = {0};
            serverAddr.sin_family = AF_INET;
            serverAddr.sin_port = htons(ADMIRAL_PORT_ADMIRAL);
            serverAddr.sin_addr.s_addr = inet_addr(ADMIRAL_HOST_ADMIRAL);

            int c = connect(socketFd, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
            if (c == -1) {
                lmp_log_print("echo", "Could not connect to admiral", LMP_PRINT_TYPE_ERROR);
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

            u16 payloadLength = strlen((char*)sendPacket.payload);

            sendPacket.payload_length = payloadLength;

            lmp_net_send_packet(socketFd, &sendPacket, &result);

            if (result.error != LMP_ERR_NONE) {
                lmp_log_print("echo", "Failed to serialize and send packet to admiral", LMP_PRINT_TYPE_ERROR);
            }

            close(socketFd);
        }
    }

    return 0;
}
