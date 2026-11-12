#include "frame.h"

#include <string.h>

enum {
    ST_HEAD1 = 0,
    ST_HEAD2,
    ST_LEN,
    ST_CMD,
    ST_SEQ,
    ST_PAYLOAD,
    ST_CRC_LO,
    ST_CRC_HI,
    ST_TAIL1,
    ST_TAIL2
};

static void parser_reset(frame_parser_t *parser)
{
    parser->state = ST_HEAD1;
    parser->len = 0;
    parser->cmd = 0;
    parser->seq = 0;
    parser->index = 0;
    parser->crc_calc = 0xFFFFu;
    parser->crc_recv = 0;
}

uint16_t frame_crc16_update(uint16_t crc, uint8_t byte)
{
    crc ^= (uint16_t)byte;
    for (uint8_t i = 0; i < 8u; i++) {
        if ((crc & 1u) != 0u) {
            crc = (uint16_t)((crc >> 1) ^ 0xA001u);
        } else {
            crc = (uint16_t)(crc >> 1);
        }
    }
    return crc;
}

uint16_t frame_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFFu;
    if (data == NULL) {
        return crc;
    }
    for (size_t i = 0; i < len; i++) {
        crc = frame_crc16_update(crc, data[i]);
    }
    return crc;
}

void frame_parser_init(frame_parser_t *parser)
{
    if (parser == NULL) {
        return;
    }
    memset(parser, 0, sizeof(*parser));
    parser_reset(parser);
}

frame_event_t frame_parser_feed(frame_parser_t *parser, uint8_t byte, frame_t *out)
{
    if (parser == NULL) {
        return FRAME_EVENT_NONE;
    }

    switch (parser->state) {
    case ST_HEAD1:
        if (byte == FRAME_HEAD1) {
            parser->state = ST_HEAD2;
        }
        break;

    case ST_HEAD2:
        if (byte == FRAME_HEAD2) {
            parser->state = ST_LEN;
        } else if (byte != FRAME_HEAD1) {
            parser->state = ST_HEAD1;
        }
        break;

    case ST_LEN:
        if (byte > FRAME_MAX_PAYLOAD) {
            parser->length_error_count++;
            parser_reset(parser);
            return FRAME_EVENT_LENGTH_ERROR;
        }
        parser->len = byte;
        parser->crc_calc = frame_crc16_update(0xFFFFu, byte);
        parser->state = ST_CMD;
        break;

    case ST_CMD:
        parser->cmd = byte;
        parser->crc_calc = frame_crc16_update(parser->crc_calc, byte);
        parser->state = ST_SEQ;
        break;

    case ST_SEQ:
        parser->seq = byte;
        parser->crc_calc = frame_crc16_update(parser->crc_calc, byte);
        parser->index = 0;
        parser->state = (parser->len == 0u) ? ST_CRC_LO : ST_PAYLOAD;
        break;

    case ST_PAYLOAD:
        parser->payload[parser->index] = byte;
        parser->index++;
        parser->crc_calc = frame_crc16_update(parser->crc_calc, byte);
        if (parser->index >= parser->len) {
            parser->state = ST_CRC_LO;
        }
        break;

    case ST_CRC_LO:
        parser->crc_recv = byte;
        parser->state = ST_CRC_HI;
        break;

    case ST_CRC_HI:
        parser->crc_recv |= (uint16_t)((uint16_t)byte << 8);
        if (parser->crc_recv != parser->crc_calc) {
            parser->crc_error_count++;
            parser_reset(parser);
            return FRAME_EVENT_CRC_ERROR;
        }
        parser->state = ST_TAIL1;
        break;

    case ST_TAIL1:
        if (byte != FRAME_TAIL1) {
            parser->tail_error_count++;
            parser_reset(parser);
            return FRAME_EVENT_TAIL_ERROR;
        }
        parser->state = ST_TAIL2;
        break;

    case ST_TAIL2:
        if (byte != FRAME_TAIL2) {
            parser->tail_error_count++;
            parser_reset(parser);
            return FRAME_EVENT_TAIL_ERROR;
        }
        if (out != NULL) {
            out->cmd = parser->cmd;
            out->seq = parser->seq;
            out->len = parser->len;
            if (parser->len > 0u) {
                memcpy(out->payload, parser->payload, parser->len);
            }
        }
        parser->ok_count++;
        parser_reset(parser);
        return FRAME_EVENT_OK;

    default:
        parser_reset(parser);
        break;
    }

    return FRAME_EVENT_NONE;
}

size_t frame_build(uint8_t *buf, size_t buf_size, uint8_t cmd, uint8_t seq,
                   const uint8_t *payload, uint8_t payload_len)
{
    size_t total;
    size_t i = 0;
    uint16_t crc;

    if (buf == NULL || payload_len > FRAME_MAX_PAYLOAD) {
        return 0;
    }
    if (payload_len > 0u && payload == NULL) {
        return 0;
    }

    total = (size_t)FRAME_OVERHEAD + (size_t)payload_len;
    if (buf_size < total) {
        return 0;
    }

    buf[i++] = FRAME_HEAD1;
    buf[i++] = FRAME_HEAD2;
    buf[i++] = payload_len;
    buf[i++] = cmd;
    buf[i++] = seq;
    if (payload_len > 0u) {
        memcpy(&buf[i], payload, payload_len);
        i += payload_len;
    }

    crc = frame_crc16(&buf[2], (size_t)payload_len + 3u);
    buf[i++] = (uint8_t)(crc & 0xFFu);
    buf[i++] = (uint8_t)(crc >> 8);
    buf[i++] = FRAME_TAIL1;
    buf[i++] = FRAME_TAIL2;

    return i;
}
