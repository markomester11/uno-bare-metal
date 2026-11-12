/*
 * Single-producer / single-consumer byte ring buffer.
 *
 * One side (e.g. an ISR) only calls rb_put, the other side only calls rb_get,
 * so no locking is needed: each index is written by exactly one side, and an
 * 8-bit read or write is atomic on AVR.
 *
 * The indices run freely from 0 to 255 and wrap. Because 256 is a multiple of
 * RB_SIZE, (index & RB_MASK) is always the right slot, and head - tail is the
 * number of stored bytes. That lets every slot be used.
 */
#ifndef RINGBUF_H
#define RINGBUF_H

#include <stdbool.h>
#include <stdint.h>

#define RB_SIZE 64u /* power of two, at most 128 */
#define RB_MASK (RB_SIZE - 1u)

#if (RB_SIZE & RB_MASK) != 0 || RB_SIZE > 128u
#error "RB_SIZE must be a power of two no larger than 128"
#endif

/* Stops the compiler from moving the data write after the index update,
 * so the other side never sees an index that points at a byte not yet written. */
#define RB_BARRIER() __asm__ __volatile__("" ::: "memory")

typedef struct {
    volatile uint8_t head; /* written only by the producer */
    volatile uint8_t tail; /* written only by the consumer */
    uint8_t data[RB_SIZE];
} ringbuf_t;

static inline void rb_init(ringbuf_t *rb)
{
    rb->head = 0;
    rb->tail = 0;
}

static inline uint8_t rb_count(const ringbuf_t *rb)
{
    return (uint8_t)(rb->head - rb->tail);
}

static inline bool rb_empty(const ringbuf_t *rb)
{
    return rb->head == rb->tail;
}

static inline bool rb_full(const ringbuf_t *rb)
{
    return rb_count(rb) == RB_SIZE;
}

/* Returns false (and drops the byte) when the buffer is full. */
static inline bool rb_put(ringbuf_t *rb, uint8_t byte)
{
    if (rb_full(rb)) {
        return false;
    }
    rb->data[rb->head & RB_MASK] = byte;
    RB_BARRIER();
    rb->head = (uint8_t)(rb->head + 1u);
    return true;
}

/* Returns false when the buffer is empty. */
static inline bool rb_get(ringbuf_t *rb, uint8_t *out)
{
    if (rb_empty(rb)) {
        return false;
    }
    *out = rb->data[rb->tail & RB_MASK];
    RB_BARRIER();
    rb->tail = (uint8_t)(rb->tail + 1u);
    return true;
}

#endif /* RINGBUF_H */
