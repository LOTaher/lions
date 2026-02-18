/*  lmp.c - LIONS Middleware Protocol
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

#include <string.h>

#include "../include/lt_base.h"
#include "../include/lmp.h"

void lmp_packet_init(lmp_packet* packet) {
    packet->version = 0;
    packet->type = 0;
    packet->arg = 0;
    packet->flags = 0;

    packet->payload = NULL;
    packet->payload_length = 0;
}

void lmp_result_init(lmp_result* result) {
    result->size = 0;
    result->error = LMP_ERR_NONE;
}

void lmp_packet_serialize(u8* buffer, size_t size, const lmp_packet* packet, lmp_result* result) {
    if (!buffer || !packet || !result) {
        result->error = LMP_ERR_BAD_INPUT;
        return;
    }

    lmp_result_init(result);

    if (!packet->payload || packet->payload_length < 1) {
        result->error = LMP_ERR_BAD_PAYLOAD;
        return;
    }

    s32 version = 0;
    for (size_t i = 0; i < ARR_LENGTH(lmp_versions); i++) {
        if (packet->version == lmp_versions[i]) {
            version = lmp_versions[i];
            break;
        }
    }

    if (!version) {
        result->error = LMP_ERR_BAD_VERSION;
        return;
    }

    if (packet->type < LMP_TYPE_INIT || packet->type > LMP_TYPE_INVALID) {
        result->error = LMP_ERR_BAD_TYPE;
        return;
    }

    switch (packet->type) {
        case LMP_TYPE_INIT:
            if (packet->arg < LMP_ARG_INIT_INIT || packet->arg > LMP_ARG_INIT_ACCEPT) {
                result->error = LMP_ERR_BAD_ARG;
                return;
            }
            break;
        case LMP_TYPE_PING:
            if (packet->arg != LMP_ARG_PING) {
                result->error = LMP_ERR_BAD_ARG;
                return;
            }
            break;
        case LMP_TYPE_SEND:
            if (packet->arg != LMP_ARG_SEND) {
                result->error = LMP_ERR_BAD_ARG;
                return;
            }
            break;
        case LMP_TYPE_TERM:
            if (packet->arg < LMP_ARG_TERM_CLEAN || packet->arg > LMP_ARG_TERM_BUSY) {
                result->error = LMP_ERR_BAD_ARG;
                return;
            }
            break;
        case LMP_TYPE_INVALID:
            if (packet->arg < LMP_ARG_INVALID_VERSION || packet->arg > LMP_ARG_INVALID_PAYLOAD) {
                result->error = LMP_ERR_BAD_ARG;
                return;
            }
            break;
    }

    if ((packet->type == LMP_TYPE_INIT || packet->type == LMP_TYPE_INVALID)) {
        if (!(packet->payload_length == 1 && packet->payload[0] == LMP_PAYLOAD_EMPTY)) {
            result->error = LMP_ERR_BAD_PAYLOAD;
            return;
        }
    }

    size_t total_size = LMP_PACKET_HEADER_SIZE + packet->payload_length + 1;

    if (total_size < LMP_PACKET_MIN_SIZE || total_size > LMP_PACKET_MAX_SIZE
        || size < total_size) {
        result->error = LMP_ERR_BAD_SIZE;
        return;
    }

    buffer[0] = version;
    buffer[1] = packet->type;
    buffer[2] = packet->arg;
    buffer[3] = packet->flags;

    memcpy(buffer + LMP_PACKET_HEADER_SIZE, packet->payload, packet->payload_length);

    buffer[LMP_PACKET_HEADER_SIZE + packet->payload_length] = LMP_PACKET_TERMINATE;

    result->size = total_size;
    result->error = LMP_ERR_NONE;
}

void lmp_packet_deserialize(const u8* buffer, size_t size, lmp_packet* packet, lmp_result* result) {
    if (!buffer || !packet || !result) {
        result->error = LMP_ERR_BAD_INPUT;
        return;
    }

    lmp_packet_init(packet);

    if (size < LMP_PACKET_MIN_SIZE || size > LMP_PACKET_MAX_SIZE) {
        result->error = LMP_ERR_BAD_SIZE;
        return;
    }

    s32 version = 0;
    for (size_t i = 0; i < ARR_LENGTH(lmp_versions); i++) {
        if (buffer[0] == lmp_versions[i]) {
            version = buffer[0];
            break;
        }
    }

    if (version == 0) {
        result->error = LMP_ERR_BAD_VERSION;
        return;
    }

    if (buffer[1] < LMP_TYPE_INIT || buffer[1] > LMP_TYPE_INVALID) {
        result->error = LMP_ERR_BAD_TYPE;
        return;
    }

    switch (buffer[1]) {
        case LMP_TYPE_INIT:
            if (buffer[2] < LMP_ARG_INIT_INIT || buffer[2] > LMP_ARG_INIT_ACCEPT) {
                result->error = LMP_ERR_BAD_ARG;
                return;
            }
            break;
        case LMP_TYPE_PING:
            if (buffer[2] != LMP_ARG_PING) {
                result->error = LMP_ERR_BAD_ARG;
                return;
            }
            break;
        case LMP_TYPE_SEND:
            if (buffer[2] != LMP_ARG_SEND) {
                result->error = LMP_ERR_BAD_ARG;
                return;
            }
            break;
        case LMP_TYPE_TERM:
            if (buffer[2] < LMP_ARG_TERM_CLEAN || buffer[2] > LMP_ARG_TERM_BUSY) {
                result->error = LMP_ERR_BAD_ARG;
                return;
            }
            break;
        case LMP_TYPE_INVALID:
            if (buffer[2] < LMP_ARG_INVALID_VERSION || buffer[2] > LMP_ARG_INVALID_PAYLOAD) {
                result->error = LMP_ERR_BAD_ARG;
                return;
            }
            break;
    }

    packet->version = version;
    packet->type = buffer[1];
    packet->arg = buffer[2];
    packet->flags = buffer[3];

    if ((buffer[1] == LMP_TYPE_INVALID || buffer[1] == LMP_TYPE_INIT)
        && buffer[LMP_PACKET_HEADER_SIZE] != LMP_PAYLOAD_EMPTY) {
        result->error = LMP_ERR_BAD_PAYLOAD;
        return;
    }

    if (buffer[size - 1] != LMP_PACKET_TERMINATE) {
        result->error = LMP_ERR_BAD_TERMINATE;
        return;
    }

    if (size < LMP_PACKET_HEADER_SIZE + 1) {
        result->error = LMP_ERR_BAD_SIZE;
        return;
    }

    size_t payload_length = size - LMP_PACKET_HEADER_SIZE - 1;

    if (payload_length < 1) {
        result->error = LMP_ERR_BAD_PAYLOAD;
        return;
    }

	if ((packet->type == LMP_TYPE_INIT || packet->type == LMP_TYPE_INVALID)
        && payload_length != 1) {
	    result->error = LMP_ERR_BAD_PAYLOAD;
	    return;
	}

	if (payload_length == 1 && buffer[LMP_PACKET_HEADER_SIZE] != LMP_PAYLOAD_EMPTY) {
	    result->error = LMP_ERR_BAD_PAYLOAD;
	    return;
	}

    packet->payload = buffer + LMP_PACKET_HEADER_SIZE;
    packet->payload_length = payload_length;

    result->size = size;
    result->error = LMP_ERR_NONE;
}
