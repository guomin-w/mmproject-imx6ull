#ifndef VIDEOPLAYAUDIO_H
#define VIDEOPLAYAUDIO_H

#include <QObject>
#include <QMutex>
#include <QWaitCondition>

#include <alsa/asoundlib.h>
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

#include "videoqueue.h"
#include "commonresource.h"


class VideoPlayAudio : public QObject
{
    Q_OBJECT
public:
    explicit VideoPlayAudio(AVFormatContext *infmt_ctx, int audio_stream_idx, VideoQueue *audio_queue, QObject *parent = nullptr);

    void pause();       // 暂停播放
    void resume();      // 恢复播放
    void stop();        // 停止播放

    void set_volume(int volume_percent);  // 音量调节槽函数

private:
    int curr_time;          // 当前播放位置
    bool is_stop;           // 停止标志位
    bool is_pause;          // 暂停标志位

    QMutex mutex;           // 互斥量
    QWaitCondition cond;    // 条件变量，用于暂停状态

    AVFormatContext *infmt_ctx;     // 格式上下文
    int audio_stream_idx;           // 音频流 id
    VideoQueue *audio_queue;        // 音频队列

    double audio_clock;     // 音频时钟，用于同步

    snd_pcm_t *pcm = nullptr;    // pcm 设备
    snd_mixer_elem_t *volume_elem = nullptr;    // 混音器中音量调节元素

    void pcm_init();             // pcm 初始化
    void mixer_init();           // 混音器初始化

    /* 静态成员 */
    static int channels;            // 声道数
    static unsigned int rate;       // 硬件设备支持的采样率，此采样率可用于ffmpeg重采样过程，把音乐文件的帧数据采样率匹配硬件采样率
    static int period_size;         // 周期大小，一个周期包含多少帧
    static int periods;             // 缓冲区大小 (周期数)，一个缓冲区包含多少周期

public slots:
    void start();   // 线程函数，开始播放

signals:
    void sig_curr_time(int curr_time);
    void sig_audio_clock(double audio_clock);
    void sig_audio_finished();
};

#endif // VIDEOPLAYAUDIO_H
