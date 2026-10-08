/*
 * 无锁 SPSC 环形队列实现。
 * 说明：put 由生产者（中断）调用，get 由消费者（主循环）调用；
 *       head/tail 使用 volatile 保证可见性，当前未使用显式内存屏障（见 AGENTS.md 风险 R7）。
 */
#include "FIFO_LOCKFREE.h"

/*
 * 功能：复位队列为空
 */
void fifo_lockfree_reset(fifo_lockfree_t *fifo)
{
    fifo->head = 0;
    fifo->tail = 0;
}

/*
 * 功能：判断队列是否为空
 * 返回：true 表示空
 */
bool fifo_lockfree_is_empty(const fifo_lockfree_t *fifo)
{
    return (fifo->head == fifo->tail);
}

/*
 * 功能：判断队列是否已满
 * 返回：true 表示满
 * 说明：采用“牺牲一个元素”的策略，可用容量为 size-1。
 */
bool fifo_lockfree_is_full(const fifo_lockfree_t *fifo)
{
    return (((fifo->tail + 1) % fifo->size) == fifo->head);
}

/*
 * 功能：写入一个元素（生产者调用）
 * 返回：true 写入成功；false 队列已满
 */
bool fifo_lockfree_put(fifo_lockfree_t *fifo, fifo_data_t data)
{
    if (fifo_lockfree_is_full(fifo))
    {
        return false; // 队列已满：丢弃本次写入（如需等待或覆盖旧数据，可在此调整）
    }
    // 先写数据，再移动写索引
    fifo->buf[fifo->tail] = data;
    fifo->tail = (fifo->tail + 1) % fifo->size;
    return true;
}

/*
 * 功能：读取一个元素（消费者调用）
 * 返回：true 读取成功；false 队列为空
 */
bool fifo_lockfree_get(fifo_lockfree_t *fifo, fifo_data_t *data)
{
    if (fifo_lockfree_is_empty(fifo))
    {
        return false;
    }
    // 先读数据，再移动读索引
    *data = fifo->buf[fifo->head];
    fifo->head = (fifo->head + 1) % fifo->size;
    return true;
}

/*
 * 功能：返回当前已用元素个数
 * 说明：已用空间 = (tail - head + size) % size
 */
uint_fast8_t fifo_lockfree_available(const fifo_lockfree_t *fifo)
{
    return (fifo->tail - fifo->head + fifo->size) % fifo->size;
}
