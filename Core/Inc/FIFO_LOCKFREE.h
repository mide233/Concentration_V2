#ifndef FIFO_LOCKFREE_H
#define FIFO_LOCKFREE_H

#include <stdint.h>
#include <stdbool.h>

/*
 * 无锁单生产者-单消费者（SPSC）环形队列。
 * 约束：读索引仅由消费者（主循环）修改，写索引仅由生产者（中断）修改；
 *       不适用于多生产者或多消费者场景。
 */

/* 队列元素类型：可在此修改为其他类型（如浮点或整数定点） */
typedef uint16_t fifo_data_t;

/* 无锁 FIFO 控制块 */
typedef struct {
    fifo_data_t *buf;           // 缓冲区指针
    uint_fast8_t size;          // 缓冲区容量（实际最多存放 size-1 个元素）
    volatile uint_fast8_t head; // 读索引（仅消费者/主循环修改）
    volatile uint_fast8_t tail; // 写索引（仅生产者/中断修改）
} fifo_lockfree_t;

/**
 * @brief  静态初始化宏
 * @param  name   FIFO 变量名
 * @param  _buf   缓冲区数组（元素类型为 fifo_data_t）
 * @param  _size  数组长度（实际最多存放 _size-1 个元素）
 */
#define FIFO_LOCKFREE_INIT(name, _buf, _size) { \
    .buf  = (_buf),                             \
    .size = (_size),                            \
    .head = 0,                                  \
    .tail = 0}

/* 对外 API */
void fifo_lockfree_reset(fifo_lockfree_t *fifo);                   // 复位队列
bool fifo_lockfree_is_empty(const fifo_lockfree_t *fifo);          // 是否为空
bool fifo_lockfree_is_full(const fifo_lockfree_t *fifo);           // 是否已满
bool fifo_lockfree_put(fifo_lockfree_t *fifo, fifo_data_t data);   // 写入一个元素（生产者调用）
bool fifo_lockfree_get(fifo_lockfree_t *fifo, fifo_data_t *data);  // 读取一个元素（消费者调用）
uint_fast8_t fifo_lockfree_available(const fifo_lockfree_t *fifo); // 已用元素个数

#endif
