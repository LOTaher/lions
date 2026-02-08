#include <string.h>

#include "../include/lt_base.h"
#include "../include/stmp.h"

void stmp_packet_init(stmp_packet* packet) {
    packet->version = 0;
    packet->type = 0;
    packet->arg = 0;
    packet->flags = 0;

    packet->payload = NULL;
    packet->payload_length = 0;
}

void stmp_result_init(stmp_result* result) {
    result->size = 0;
    result->error = STMP_ERR_NONE;
}

void stmp_packet_serialize(u8* buffer, size_t size, const stmp_packet* packet, stmp_result* result) {
    if (!buffer || !packet || !result) {
        result->error = STMP_ERR_BAD_INPUT;
        return;
    }

    stmp_result_init(result);

    if (!packet->payload || packet->payload_length < 1) {
        result->error = STMP_ERR_BAD_PAYLOAD;
        return;
    }

    s32 version = 0;
    for (size_t i = 0; i < ARR_LENGTH(stmp_versions); i++) {
        if (packet->version == stmp_versions[i]) {
            version = stmp_versions[i];
            break;
        }
    }

    if (!version) {
        result->error = STMP_ERR_BAD_VERSION;
        return;
    }

    if (packet->type < STMP_TYPE_INIT || packet->type > STMP_TYPE_INVALID) {
        result->error = STMP_ERR_BAD_TYPE;
        return;
    }

    switch (packet->type) {
        case STMP_TYPE_INIT:
            if (packet->arg < STMP_ARG_INIT_INIT || packet->arg > STMP_ARG_INIT_ACCEPT) {
                result->error = STMP_ERR_BAD_ARG;
                return;
            }
            break;
        case STMP_TYPE_PING:
            if (packet->arg != STMP_ARG_PING) {
                result->error = STMP_ERR_BAD_ARG;
                return;
            }
            break;
        case STMP_TYPE_SEND:
            if (packet->arg != STMP_ARG_SEND) {
                result->error = STMP_ERR_BAD_ARG;
                return;
            }
            break;
        case STMP_TYPE_TERM:
            if (packet->arg < STMP_ARG_TERM_CLEAN || packet->arg > STMP_ARG_TERM_BUSY) {
                result->error = STMP_ERR_BAD_ARG;
                return;
            }
            break;
        case STMP_TYPE_INVALID:
            if (packet->arg < STMP_ARG_INVALID_VERSION || packet->arg > STMP_ARG_INVALID_PAYLOAD) {
                result->error = STMP_ERR_BAD_ARG;
                return;
            }
            break;
    }

    if ((packet->type == STMP_TYPE_INIT || packet->type == STMP_TYPE_INVALID)) {
        if (!(packet->payload_length == 1 && packet->payload[0] == STMP_PAYLOAD_EMPTY)) {
            result->error = STMP_ERR_BAD_PAYLOAD;
            return;
        }
    }

    size_t total_size = STMP_PACKET_HEADER_SIZE + packet->payload_length + 1;

    if (total_size < STMP_PACKET_MIN_SIZE || total_size > STMP_PACKET_MAX_SIZE
        || size < total_size) {
        result->error = STMP_ERR_BAD_SIZE;
        return;
    }

    buffer[0] = version;
    buffer[1] = packet->type;
    buffer[2] = packet->arg;
    buffer[3] = packet->flags;

    memcpy(buffer + STMP_PACKET_HEADER_SIZE, packet->payload, packet->payload_length);

    buffer[STMP_PACKET_HEADER_SIZE + packet->payload_length] = STMP_PACKET_TERMINATE;

    result->size = total_size;
    result->error = STMP_ERR_NONE;
}

void stmp_packet_deserialize(const u8* buffer, size_t size, stmp_packet* packet, stmp_result* result) {
    if (!buffer || !packet || !result) {
        result->error = STMP_ERR_BAD_INPUT;
        return;
    }

    stmp_packet_init(packet);

    if (size < STMP_PACKET_MIN_SIZE || size > STMP_PACKET_MAX_SIZE) {
        result->error = STMP_ERR_BAD_SIZE;
        return;
    }

    s32 version = 0;
    for (size_t i = 0; i < ARR_LENGTH(stmp_versions); i++) {
        if (buffer[0] == stmp_versions[i]) {
            version = buffer[0];
            break;
        }
    }

    if (version == 0) {
        result->error = STMP_ERR_BAD_VERSION;
        return;
    }

    if (buffer[1] < STMP_TYPE_INIT || buffer[1] > STMP_TYPE_INVALID) {
        result->error = STMP_ERR_BAD_TYPE;
        return;
    }

    switch (buffer[1]) {
        case STMP_TYPE_INIT:
            if (buffer[2] < STMP_ARG_INIT_INIT || buffer[2] > STMP_ARG_INIT_ACCEPT) {
                result->error = STMP_ERR_BAD_ARG;
                return;
            }
            break;
        case STMP_TYPE_PING:
            if (buffer[2] != STMP_ARG_PING) {
                result->error = STMP_ERR_BAD_ARG;
                return;
            }
            break;
        case STMP_TYPE_SEND:
            if (buffer[2] != STMP_ARG_SEND) {
                result->error = STMP_ERR_BAD_ARG;
                return;
            }
            break;
        case STMP_TYPE_TERM:
            if (buffer[2] < STMP_ARG_TERM_CLEAN || buffer[2] > STMP_ARG_TERM_BUSY) {
                result->error = STMP_ERR_BAD_ARG;
                return;
            }
            break;
        case STMP_TYPE_INVALID:
            if (buffer[2] < STMP_ARG_INVALID_VERSION || buffer[2] > STMP_ARG_INVALID_PAYLOAD) {
                result->error = STMP_ERR_BAD_ARG;
                return;
            }
            break;
    }

    packet->version = version;
    packet->type = buffer[1];
    packet->arg = buffer[2];
    packet->flags = buffer[3];

    if ((buffer[1] == STMP_TYPE_INVALID || buffer[1] == STMP_TYPE_INIT)
        && buffer[STMP_PACKET_HEADER_SIZE] != STMP_PAYLOAD_EMPTY) {
        result->error = STMP_ERR_BAD_PAYLOAD;
        return;
    }

    if (buffer[size - 1] != STMP_PACKET_TERMINATE) {
        result->error = STMP_ERR_BAD_TERMINATE;
        return;
    }

    if (size < STMP_PACKET_HEADER_SIZE + 1) {
        result->error = STMP_ERR_BAD_SIZE;
        return;
    }

    size_t payload_length = size - STMP_PACKET_HEADER_SIZE - 1;

    if (payload_length < 1) {
        result->error = STMP_ERR_BAD_PAYLOAD;
        return;
    }

	if ((packet->type == STMP_TYPE_INIT || packet->type == STMP_TYPE_INVALID)
        && payload_length != 1) {
	    result->error = STMP_ERR_BAD_PAYLOAD;
	    return;
	}

	if (payload_length == 1 && buffer[STMP_PACKET_HEADER_SIZE] != STMP_PAYLOAD_EMPTY) {
	    result->error = STMP_ERR_BAD_PAYLOAD;
	    return;
	}

    packet->payload = buffer + STMP_PACKET_HEADER_SIZE;
    packet->payload_length = payload_length;

    result->size = size;
    result->error = STMP_ERR_NONE;
}
