#ifndef USB_UART_RING_H
#define USB_UART_RING_H
#include <stdint.h>
/* Single producer/single consumer; power-of-two capacity. Full capacity usable. */
typedef struct {
    uint8_t *data;
    uint32_t capacity;
    volatile uint32_t write;
    volatile uint32_t read;
} ByteRing;
static inline uint32_t ring_count(const ByteRing *r) { return r->write - r->read; }
static inline uint32_t ring_free(const ByteRing *r) { return r->capacity - ring_count(r); }
static inline int ring_push(ByteRing *r, uint8_t byte) {
    if (!ring_free(r)) return 0;
    r->data[r->write & (r->capacity - 1)] = byte;
    __asm__ volatile("" ::: "memory");
    r->write++;
    return 1;
}
static inline int ring_pop(ByteRing *r, uint8_t *byte) {
    if (!ring_count(r)) return 0;
    *byte = r->data[r->read & (r->capacity - 1)];
    __asm__ volatile("" ::: "memory");
    r->read++;
    return 1;
}
#endif
