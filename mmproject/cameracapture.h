#ifndef CAMERACAPTURE_H
#define CAMERACAPTURE_H

#include <QObject>
#include <QMutex>
#include <QMutexLocker>
#include "commonresource.h"


class CameraCapture : public QObject
{
    Q_OBJECT
public:
    explicit CameraCapture(QObject *parent = nullptr);
    ~CameraCapture();

    void stop();    // 停止采集

public slots:
    void start();   // 启动采集

private:
    int fd;                        /* 摄像头设备的文件句柄 */
    unsigned char *mmap_addr[4];   /* 存储摄像头缓冲区队列buffer映射后的首地址 */
    unsigned int addr_length[4];   /* 存储摄像头缓冲区队列buffer映射后的空间大小 */

    bool isCanRun;                 /* 标志位 */
    QMutex lock;                   /* 互斥锁 */

    int cameraInit();

signals:
    void frameReady(const QByteArray& frameData);      // 发送摄像头数据
};

#endif // CAMERACAPTURE_H
