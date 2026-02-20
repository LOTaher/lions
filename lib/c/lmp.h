/*  lmp.h - LIONS Middleware Protocol
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

#ifndef LMP_H
#define LMP_H

#include <stddef.h>
#include <stdint.h>

#include "lt_base.h"

typedef u8 lmp_version;
typedef u8 lmp_type;
typedef u8 lmp_arg;
typedef u8 lmp_flag;

/* [0] Version */
static const lmp_version lmp_versions[] = {1, 2};

/* [1] Type */
#define LMP_TYPE_INIT 0x01
#define LMP_TYPE_PING 0x02
#define LMP_TYPE_SEND 0x03
#define LMP_TYPE_TERM 0x04
#define LMP_TYPE_INVALID 0x05

/* [2] Argument */
#define LMP_ARG_INIT_INIT 0x01
#define LMP_ARG_INIT_ACCEPT 0x02
#define LMP_ARG_PING 0x00
#define LMP_ARG_SEND 0x00
#define LMP_ARG_TERM_CLEAN 0x01
#define LMP_ARG_TERM_BUSY 0x02
#define LMP_ARG_INVALID_VERSION 0x01
#define LMP_ARG_INVALID_TYPE 0x02
#define LMP_ARG_INVALID_MESSAGE 0x03
#define LMP_ARG_INVALID_ARGUMENT 0x04
#define LMP_ARG_INVALID_FLAGS 0x05
#define LMP_ARG_INVALID_PAYLOAD 0x06

/* [3] Flags */
#define LMP_FLAGS_NONE 0
#define LMP_FLAGS_LOG (1 << 0)
#define LMP_FLAGS_INCOGNITO (1 << 1)

/* [4] Payload */
#define LMP_PAYLOAD_EMPTY 0x00

/* Packet */
#define LMP_PACKET_HEADER_SIZE 0x04 // 4
#define LMP_PACKET_MAX_SIZE 0x5DC // 1500
#define LMP_PACKET_MIN_SIZE 0x05 // 5
#define LMP_PACKET_TERMINATE 0x7F
#define LMP_PACKET_PAYLOAD_MAX_SIZE 0x5D7 // 1495

typedef enum {
    LMP_ERR_NONE,
    LMP_ERR_BAD_SIZE,
    LMP_ERR_BAD_VERSION,
    LMP_ERR_BAD_TYPE,
    LMP_ERR_BAD_ARG,
    LMP_ERR_BAD_PAYLOAD,
    LMP_ERR_BAD_TERMINATE,
    LMP_ERR_BAD_INPUT
} lmp_error;

typedef struct {
    lmp_version version;
    lmp_type type;
    lmp_arg arg;
    lmp_flag flags;

    const u8* payload;
    size_t payload_length;
} lmp_packet;

typedef struct {
    size_t size;
    lmp_error error;
} lmp_result;

void lmp_packet_init(lmp_packet* packet);
void lmp_result_init(lmp_result* result);
void lmp_packet_serialize(u8* buffer, size_t size, const lmp_packet* packet, lmp_result* result);
void lmp_packet_deserialize(const u8* buffer, size_t size, lmp_packet* packet, lmp_result* result);

#endif // LMP_H
