#ifndef VIDEOPLAYVIDEO_H
#define VIDEOPLAYVIDEO_H

#include <QObject>
#include <QMutex>
#include <QWaitCondition>
#include <QThread>

extern "C"
{
#include <libavutil/time.h>
#include <libavutil/imgutils.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/log.h>
}

#include "videoqueue.h"

#include <QSharedMemory>


class VideoPlayVideo : public QObject
{
    Q_OBJECT
public:
    explicit VideoPlayVideo(AVFormatContext *infmt_ctx, int video_stream_idx, VideoQueue *video_queue, QObject *parent = nullptr);

    void pause();       // 暂停播放
    void resume();      // 恢复播放
    void stop();        // 停止播放

    void set_clock(double sync_clock);    // 同步至音频时钟

private:
    int curr_time;          // 当前播放位置
    bool is_stop = false;           // 停止标志位
    bool is_pause = false;          // 暂停标志位

    QMutex mutex;           // 互斥量
    QWaitCondition cond;    // 条件变量，用于暂停状态

    AVFormatContext *infmt_ctx;     // 格式上下文
    int video_stream_idx;           // 视频流 id
    VideoQueue *video_queue;        // 视频队列

    double audio_clock = 0.0;     // 音频时钟，用于同步

public slots:
    void start();   // 线程函数，开始播放

signals:
    void sig_frame_ready(int linesize);    // 图像帧就绪信号
    void sig_video_finished();
};

#endif // VIDEOPLAYVIDEO_H
