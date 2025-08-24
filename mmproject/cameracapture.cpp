#include "cameracapture.h"
#include <QDebug>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/videodev2.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>


CameraCapture::CameraCapture(QObject *parent) : QObject(parent)
{
    cameraInit();    // V4L2 初始化
}

CameraCapture::~CameraCapture()
{
    /* munmap */
    for (int i = 0; i < 4; i++) {
        int ret = munmap(mmap_addr[i], addr_length[i]);
        if (ret < 0) {
            qDebug() << "munmap失败" << endl;
        }
    }

    /* 关闭设备文件 */
    close(fd);
}

int CameraCapture::cameraInit()
{
    int ret;

    /* 1.打开摄像头设备 */
    fd = open(CAMERA_DEVICE, O_RDWR);
    if (fd < 0) {
        qDebug() << "打开摄像头设备失败" << endl;
        return -1;
    }

    /* 2.设置摄像头采集格式 */
    struct v4l2_format fmt;
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  // 捕获设备
    fmt.fmt.pix.width = 640;
    fmt.fmt.pix.height = 400;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;

    ret = ioctl(fd, VIDIOC_S_FMT, &fmt);
    if (ret < 0) {
        qDebug() << "设置摄像头采集格式失败" << endl;
        return -1;
    }

    memset(&fmt, 0, sizeof(struct v4l2_format));  // 先清空内容，再读取格式查看是否设置成功
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ret = ioctl(fd, VIDIOC_G_FMT, &fmt);
    if (ret < 0) {
        qDebug() << "读取采集格式失败" << endl;
        return -1;
    }

    /* 3.申请缓冲队列buffer */
    struct v4l2_requestbuffers req_buffer;
    req_buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req_buffer.memory = V4L2_MEMORY_MMAP;  // 内存映射方式
    req_buffer.count = 4;                  // 申请4个buffer

    ret = ioctl(fd, VIDIOC_REQBUFS, &req_buffer);
    if (ret < 0) {
        qDebug() << "申请缓冲队列失败" << endl;
        return -1;
    }
    /* 申请成功后，映射这些buffer */
    struct v4l2_buffer map_buffer;
    map_buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    for (unsigned int i = 0; i < req_buffer.count; i++) {        // 真正申请到的buffer数量存在count属性中
        map_buffer.index = i;
        ret = ioctl(fd, VIDIOC_QUERYBUF, &map_buffer);  // 查询buffer信息
        if (ret < 0) {
            qDebug() << "查询缓冲队列失败" << endl;
            return -1;
        }
        /* 映射 */
        mmap_addr[i] = (unsigned char *)mmap(NULL, map_buffer.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, map_buffer.m.offset);
        addr_length[i] = map_buffer.length;
        /* 放入队列(空闲链表) */
        ret = ioctl(fd, VIDIOC_QBUF, &map_buffer);
        if (ret < 0) {
            qDebug() << "放入队列失败" << endl;
            return -1;
        }
    }

    return 0;
}

/* 与线程启动信号相关联 */
void CameraCapture::start()
{
    int ret;
    struct v4l2_buffer read_buffer;
    int type;

    /* 启动摄像头捕获 */
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ret = ioctl(fd, VIDIOC_STREAMON, &type);
    if (ret < 0) {
        qDebug() << "启动摄像头捕获失败" << endl;
        return ;
    }

    {   // {}作用是QMutexLocker与lock的作用范围, 确保其自动释放
        QMutexLocker locker(&lock);
        isCanRun = true;
    }

    while(1) {
        {
            QMutexLocker locker(&lock);
            if (!isCanRun) {
                break;
            }
        }
        /* 从队列中提取一帧数据 */
        read_buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        read_buffer.memory = V4L2_MEMORY_MMAP;
        ret = ioctl(fd, VIDIOC_DQBUF, &read_buffer);  /* 出队列后根据索引read_buffer.index匹配到内核中的缓冲区，找到相应映射地址mmap_addr[read_buffer.index] */
        if (ret < 0) {
            qDebug() << "读取摄像头数据失败" << endl;
        }

        /* 拷贝数据, 并发送信号 */
        QByteArray frame((const char *)(mmap_addr[read_buffer.index]), read_buffer.length);
        emit frameReady(frame);

        /* 读取数据后将buffer放回队列 */
        ret = ioctl(fd, VIDIOC_QBUF, &read_buffer);
        if (ret < 0) {
            qDebug() << "缓冲buffer放回队列失败" << endl;
        }
    }

    /* 跳出循环后，停止捕获 */
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ret = ioctl(fd, VIDIOC_STREAMOFF, &type);
    if (ret < 0) {
        qDebug() << "停止捕获失败" << endl;
    }
}

/* 停止捕获 */
void CameraCapture::stop()
{
    {
        QMutexLocker locker(&lock);
        isCanRun = false;
    }
}
