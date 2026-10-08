#include "FIFO_LOCKFREE.h"

void fifo_lockfree_reset(fifo_lockfree_t *f)
{
    f->head = 0;
    f->tail = 0;
}

bool fifo_lockfree_is_empty(const fifo_lockfree_t *f)
{
    return (f->head == f->tail);
}

bool fifo_lockfree_is_full(const fifo_lockfree_t *f)
{
    return (((f->tail + 1) % f->size) == f->head);
}

bool fifo_lockfree_put(fifo_lockfree_t *f, fifo_data_t data)
{
    if (fifo_lockfree_is_full(f)) {
        return false;   /* 满，丢弃（可根据需要等待或覆盖） */
    }
    /* 先写数据，再移动尾指针 */
    f->buf[f->tail] = data;
    f->tail = (f->tail + 1) % f->size;
    return true;
}

bool fifo_lockfree_get(fifo_lockfree_t *f, fifo_data_t *data)
{
    if (fifo_lockfree_is_empty(f)) {
        return false;
    }
    /* 先读数据，再移动头指针 */
    *data = f->buf[f->head];
    f->head = (f->head + 1) % f->size;
    return true;
}

uint_fast8_t fifo_lockfree_available(const fifo_lockfree_t *f)
{
    /* 已用空间 = (tail - head + size) % size */
    return (f->tail - f->head + f->size) % f->size;
}