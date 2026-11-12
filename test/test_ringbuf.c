/* Host-side unit tests for ringbuf.h. Run with: make test */
#include <stdio.h>
#include <stdlib.h>

#include "ringbuf.h"

static int checks;

#define CHECK(cond)                                                       \
    do {                                                                  \
        checks++;                                                         \
        if (!(cond)) {                                                    \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__,        \
                    __LINE__, #cond);                                     \
            exit(1);                                                      \
        }                                                                 \
    } while (0)

static void test_starts_empty(void)
{
    ringbuf_t rb;
    uint8_t out;
    rb_init(&rb);
    CHECK(rb_empty(&rb));
    CHECK(!rb_full(&rb));
    CHECK(rb_count(&rb) == 0);
    CHECK(!rb_get(&rb, &out));
}

static void test_fifo_order(void)
{
    ringbuf_t rb;
    uint8_t out;
    rb_init(&rb);
    for (uint8_t i = 0; i < 10; i++) {
        CHECK(rb_put(&rb, i));
    }
    CHECK(rb_count(&rb) == 10);
    for (uint8_t i = 0; i < 10; i++) {
        CHECK(rb_get(&rb, &out));
        CHECK(out == i);
    }
    CHECK(rb_empty(&rb));
}

static void test_full_buffer_rejects_and_keeps_data(void)
{
    ringbuf_t rb;
    uint8_t out;
    rb_init(&rb);
    for (unsigned i = 0; i < RB_SIZE; i++) {
        CHECK(rb_put(&rb, (uint8_t)i));
    }
    CHECK(rb_full(&rb));
    CHECK(rb_count(&rb) == RB_SIZE);
    CHECK(!rb_put(&rb, 0xAA)); /* every slot is used, so this must fail */
    for (unsigned i = 0; i < RB_SIZE; i++) {
        CHECK(rb_get(&rb, &out));
        CHECK(out == (uint8_t)i);
    }
    CHECK(rb_empty(&rb));
}

/* Push 10,000 bytes through in uneven batches so both indices wrap past 255 many times. */
static void test_index_wraparound(void)
{
    ringbuf_t rb;
    uint8_t out;
    uint32_t written = 0, read = 0;
    rb_init(&rb);
    while (read < 10000u) {
        for (int k = 0; k < 7 && rb_put(&rb, (uint8_t)(written * 31u)); k++) {
            written++;
        }
        for (int k = 0; k < 5 && rb_get(&rb, &out); k++) {
            CHECK(out == (uint8_t)(read * 31u));
            read++;
        }
        CHECK(rb_count(&rb) == (uint8_t)(written - read));
    }
}

int main(void)
{
    test_starts_empty();
    test_fifo_order();
    test_full_buffer_rejects_and_keeps_data();
    test_index_wraparound();
    printf("ringbuf: all %d checks passed\n", checks);
    return 0;
}
