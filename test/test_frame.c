#include <stdio.h>
#include <string.h>

#include "frame.h"

static int failures = 0;

#define CHECK(cond, msg)                                                     \
    do {                                                                     \
        if (!(cond)) {                                                       \
            printf("FAIL: %s (line %d)\n", (msg), __LINE__);                 \
            failures++;                                                      \
        }                                                                    \
    } while (0)

static void test_crc_known_vector(void)
{
    static const uint8_t data[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    CHECK(frame_crc16(data, sizeof(data)) == 0x4B37u, "CRC known vector");
}

static void test_round_trip(void)
{
    uint8_t buf[64];
    const uint8_t payload[] = {0x11, 0x22, 0x33};
    frame_parser_t parser;
    frame_t frame;
    frame_event_t event = FRAME_EVENT_NONE;
    size_t len;

    len = frame_build(buf, sizeof(buf), 0x02u, 0x07u, payload, sizeof(payload));
    CHECK(len == FRAME_OVERHEAD + sizeof(payload), "build length");

    frame_parser_init(&parser);
    for (size_t i = 0; i < len; i++) {
        event = frame_parser_feed(&parser, buf[i], &frame);
    }

    CHECK(event == FRAME_EVENT_OK, "round trip event");
    CHECK(frame.cmd == 0x02u, "round trip cmd");
    CHECK(frame.seq == 0x07u, "round trip seq");
    CHECK(frame.len == sizeof(payload), "round trip len");
    CHECK(memcmp(frame.payload, payload, sizeof(payload)) == 0, "round trip payload");
}

static void test_garbage_prefix(void)
{
    uint8_t buf[64];
    frame_parser_t parser;
    frame_t frame;
    frame_event_t event = FRAME_EVENT_NONE;
    size_t len;
    static const uint8_t garbage[] = {0x00, 0xAA, 0x00, 0xAA};

    len = frame_build(buf, sizeof(buf), 0x01u, 0x01u, NULL, 0);
    CHECK(len > 0, "build empty payload");

    frame_parser_init(&parser);
    for (size_t i = 0; i < sizeof(garbage); i++) {
        (void)frame_parser_feed(&parser, garbage[i], &frame);
    }
    for (size_t i = 0; i < len; i++) {
        event = frame_parser_feed(&parser, buf[i], &frame);
    }

    CHECK(event == FRAME_EVENT_OK, "resync after garbage");
    CHECK(frame.cmd == 0x01u, "garbage prefix cmd");
}

static void test_bad_crc(void)
{
    uint8_t buf[64];
    const uint8_t payload[] = {0x10, 0x20};
    frame_parser_t parser;
    frame_t frame;
    frame_event_t event = FRAME_EVENT_NONE;
    size_t len;

    len = frame_build(buf, sizeof(buf), 0x02u, 0x03u, payload, sizeof(payload));
    CHECK(len > 0, "bad crc build");
    buf[5] ^= 0xFFu;

    frame_parser_init(&parser);
    for (size_t i = 0; i < len; i++) {
        event = frame_parser_feed(&parser, buf[i], &frame);
        if (event == FRAME_EVENT_CRC_ERROR) {
            break;
        }
    }

    CHECK(event == FRAME_EVENT_CRC_ERROR, "bad crc event");
    CHECK(parser.crc_error_count == 1u, "bad crc counter");
}

static void test_bad_tail(void)
{
    uint8_t buf[64];
    const uint8_t payload[] = {0x10};
    frame_parser_t parser;
    frame_t frame;
    frame_event_t event = FRAME_EVENT_NONE;
    size_t len;

    len = frame_build(buf, sizeof(buf), 0x02u, 0x04u, payload, sizeof(payload));
    CHECK(len > 0, "bad tail build");
    buf[len - 1u] = 0x00u;

    frame_parser_init(&parser);
    for (size_t i = 0; i < len; i++) {
        event = frame_parser_feed(&parser, buf[i], &frame);
        if (event == FRAME_EVENT_TAIL_ERROR) {
            break;
        }
    }

    CHECK(event == FRAME_EVENT_TAIL_ERROR, "bad tail event");
    CHECK(parser.tail_error_count == 1u, "bad tail counter");
}

static void test_length_error(void)
{
    frame_parser_t parser;
    frame_t frame;
    frame_event_t event;
    const uint8_t bytes[] = {FRAME_HEAD1, FRAME_HEAD2, 0x41u};

    frame_parser_init(&parser);
    event = frame_parser_feed(&parser, bytes[0], &frame);
    CHECK(event == FRAME_EVENT_NONE, "length error head1");
    event = frame_parser_feed(&parser, bytes[1], &frame);
    CHECK(event == FRAME_EVENT_NONE, "length error head2");
    event = frame_parser_feed(&parser, bytes[2], &frame);

    CHECK(event == FRAME_EVENT_LENGTH_ERROR, "length error event");
    CHECK(parser.length_error_count == 1u, "length error counter");
}

static void test_resync_after_bad_frame(void)
{
    uint8_t bad[64];
    uint8_t good[64];
    frame_parser_t parser;
    frame_t frame;
    frame_event_t event = FRAME_EVENT_NONE;
    size_t bad_len;
    size_t good_len;

    bad_len = frame_build(bad, sizeof(bad), 0x02u, 0x05u, NULL, 0);
    good_len = frame_build(good, sizeof(good), 0x01u, 0x06u, NULL, 0);
    CHECK(bad_len > 0 && good_len > 0, "resync build");
    bad[4] ^= 0xFFu;

    frame_parser_init(&parser);
    for (size_t i = 0; i < bad_len; i++) {
        (void)frame_parser_feed(&parser, bad[i], &frame);
    }
    for (size_t i = 0; i < good_len; i++) {
        event = frame_parser_feed(&parser, good[i], &frame);
    }

    CHECK(event == FRAME_EVENT_OK, "resync after bad frame");
    CHECK(frame.cmd == 0x01u, "resync cmd");
}

int main(void)
{
    test_crc_known_vector();
    test_round_trip();
    test_garbage_prefix();
    test_bad_crc();
    test_bad_tail();
    test_length_error();
    test_resync_after_bad_frame();

    if (failures == 0) {
        printf("ALL TESTS PASSED\n");
        return 0;
    }

    printf("%d TEST(S) FAILED\n", failures);
    return 1;
}
