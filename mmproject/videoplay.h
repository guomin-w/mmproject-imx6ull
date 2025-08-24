#ifndef VIDEOPLAY_H
#define VIDEOPLAY_H

#include <QObject>
#include <QMutex>
#include <QWaitCondition>
#include <QString>
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
}

#include "videoplayaudio.h"
#include "videoplayvideo.h"
#include "videoqueue.h"


class VideoPlay : public QObject
{
    Q_OBJECT
public:
    explicit VideoPlay(const QString &video_fileName, QObject *parent = nullptr);

    void pause();       // 暂停播放
    void resume();      // 恢复播放
    void stop();        // 停止播放

    void set_volume(int volume_percent);  // 音量调节槽函数

private:
    QString video_file;     // 视频文件路径
    AVFormatContext *infmt_ctx = nullptr;   // 格式上下文
    int video_stream_idx = -1;              // 视频流 id
    int audio_stream_idx = -1;              // 音频流 id

    int total_time;         // 总时长
    int curr_time;          // 当前播放位置
    bool is_stop;           // 停止标志位
    bool is_pause;          // 暂停标志位
    QMutex mutex;           // 互斥量
    QWaitCondition cond;    // 条件变量，用于暂停状态

    QThread *thread_audio = nullptr;  // 音频子线程
    QThread *thread_video = nullptr;  // 视频子线程

    VideoPlayAudio *play_audio = nullptr;  // 音频播放对象
    VideoPlayVideo *play_video = nullptr;  // 视频播放对象

    VideoQueue *audio_queue;    // 音频队列
    VideoQueue *video_queue;    // 视频队列

    bool audio_finished = false;    // 音频解析结束
    bool video_finished = false;    // 视频解析结束

public slots:
    void start();   // 线程函数，开始播放

private slots:      // 从下层子线程中获取播放状态
    void slot_curr_time(int curr_time);
    void slot_audio_clock(double audio_clock);   
    void slot_ready_frame(int linesize);  // 图像帧就绪槽函数
    void slot_audio_finished();
    void slot_video_finished();

signals:            // 向上层报告播放状态
    void sig_play_complete();               // 播放完毕信号
    void sig_total_time(int total_time);    // 总时长
    void sig_curr_time(int curr_time);      // 当前播放位置
    void sig_ready_frame(int linesize);     // 图像帧就绪信号
};

#endif // VIDEOPLAY_H
