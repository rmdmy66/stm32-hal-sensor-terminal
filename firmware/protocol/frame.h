#ifndef FRAME_H
#define FRAME_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FRAME_HEAD1 0xAAu
#define FRAME_HEAD2 0x55u
#define FRAME_TAIL1 0x0Du
#define FRAME_TAIL2 0x0Au
#define FRAME_MAX_PAYLOAD 64u
#define FRAME_OVERHEAD 9u

typedef enum {
    FRAME_EVENT_NONE = 0,
    FRAME_EVENT_OK = 1,
    FRAME_EVENT_CRC_ERROR = -1,
    FRAME_EVENT_TAIL_ERROR = -2,
    FRAME_EVENT_LENGTH_ERROR = -3
} frame_event_t;

typedef struct {
    uint8_t cmd;
    uint8_t seq;
    uint8_t len;
    uint8_t payload[FRAME_MAX_PAYLOAD];
} frame_t;

typedef struct {
    uint8_t state;
    uint8_t len;
    uint8_t cmd;
    uint8_t seq;
    uint8_t index;
    uint8_t payload[FRAME_MAX_PAYLOAD];
    uint16_t crc_calc;
    uint16_t crc_recv;
    uint32_t ok_count;
    uint32_t crc_error_count;
    uint32_t tail_error_count;
    uint32_t length_error_count;
} frame_parser_t;

uint16_t frame_crc16_update(uint16_t crc, uint8_t byte);
uint16_t frame_crc16(const uint8_t *data, size_t len);

void frame_parser_init(frame_parser_t *parser);
frame_event_t frame_parser_feed(frame_parser_t *parser, uint8_t byte, frame_t *out);

size_t frame_build(uint8_t *buf, size_t buf_size, uint8_t cmd, uint8_t seq,
                   const uint8_t *payload, uint8_t payload_len);

#ifdef __cplusplus
}
#endif

#endif
