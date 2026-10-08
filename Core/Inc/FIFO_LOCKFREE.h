#ifndef FIFO_LOCKFREE_H
#define FIFO_LOCKFREE_H

#include <stdint.h>
#include <stdbool.h>

/* 用户可以在此修改为其他浮点类型（如 double），或改用整数定点数 */
typedef uint16_t fifo_data_t;

/* 无锁FIFO控制块 */
typedef struct {
    fifo_data_t *buf;           /* 缓冲区指针 */
    uint_fast8_t size;          /* 容量（实际可用 size-1） */
    volatile uint_fast8_t head; /* 读索引（主循环修改） */
    volatile uint_fast8_t tail; /* 写索引（中断修改） */
} fifo_lockfree_t;

/**
 * @brief 静态初始化宏
 * @param  name   FIFO变量名
 * @param  _buf   缓冲区数组（类型为 fifo_data_t）
 * @param  _size  数组长度（实际存储 _size-1 个元素）
 */
#define FIFO_LOCKFREE_INIT(name, _buf, _size) { \
    .buf  = (_buf),                             \
    .size = (_size),                            \
    .head = 0,                                  \
    .tail = 0}

/* API */
void fifo_lockfree_reset(fifo_lockfree_t *f);
bool fifo_lockfree_is_empty(const fifo_lockfree_t *f);
bool fifo_lockfree_is_full(const fifo_lockfree_t *f);
bool fifo_lockfree_put(fifo_lockfree_t *f, fifo_data_t data);
bool fifo_lockfree_get(fifo_lockfree_t *f, fifo_data_t *data);
uint_fast8_t fifo_lockfree_available(const fifo_lockfree_t *f);

#endif