#include "videoqueue.h"

VideoQueue::VideoQueue()
{

}

void VideoQueue::queue_push(AVPacket *pkt)
{
    AVPacketList *pktl = (AVPacketList *)av_malloc(sizeof(AVPacketList));
    pktl->pkt = *pkt;
    pktl->next = nullptr;

    QMutexLocker locker(&mutex);

    if (!last_pkt) {        // 队列为空
        first_pkt = pktl;
    } else {
        last_pkt->next = pktl;    // 加入队尾
    }
    last_pkt = pktl;    // 更新队尾指针
    nb_packets++;
    cond.wakeAll();     // 唤醒所有等待线程
}

int VideoQueue::queue_pop(AVPacket *pkt)
{
    AVPacketList *pktl = nullptr;
    int ret = 0;

    QMutexLocker locker(&mutex);

    while (1) {
        pktl = first_pkt;
        if (pktl) {
            first_pkt = pktl->next;     // 更新头指针
            if (!first_pkt) {           // 队列变为空
                last_pkt = nullptr;
            }
            nb_packets--;
            *pkt = pktl->pkt;    // 复制数据包到输出参数
            av_free(pktl);       // 释放资源
            ret = 1;
            break;               // 退出
        } else {
            cond.wait(&mutex);   // 释放锁并等待条件变量
        }
    }

    return ret;
}

void VideoQueue::queue_flush()
{
    QMutexLocker locker(&mutex);

    AVPacketList *pktl = first_pkt;
    while (pktl != nullptr) {
        AVPacketList *next = pktl->next;  // 保存下一个节点指针
        av_packet_unref(&pktl->pkt);      // 释放AVPacket内部资源
        av_free(pktl);                    // 释放节点内存
        pktl = next;
    }
    // 重置队列状态
    first_pkt = nullptr;
    last_pkt = nullptr;
    nb_packets = 0;
}
