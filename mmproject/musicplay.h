#ifndef MUSICPLAY_H
#define MUSICPLAY_H

#include <QObject>
#include <QMutex>
#include <QWaitCondition>
#include <QString>

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

class MusicPlay : public QObject
{
    Q_OBJECT
public:
    explicit MusicPlay(const QString &music_fileName, QObject *parent = nullptr);

    void pause();       // 暂停播放
    void resume();      // 恢复播放
    void stop();        // 停止播放

    void set_seek(int play_position);     // 跳转播放位置
    void set_volume(int volume_percent);  // 音量调节槽函数

private:
    QString music_file;          // 音乐文件路径

    int total_time;         // 总时长
    int curr_time;          // 当前播放位置
    int seek_position;      // 跳转播放位置
    bool is_stop;           // 停止标志位
    bool is_pause;          // 暂停标志位
    bool need_seek;         // 需要跳转标志位
    QMutex mutex;           // 互斥量
    QWaitCondition cond;    // 条件变量，用于暂停状态

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
    void start();   // 线程函数，播放音乐

signals:
    void play_complete();   // 一首歌曲播放完毕信号
    void send_total_time(int total_time);    // 音乐总时长
    void send_curr_time(int curr_time);      // 当前播放位置
};

#endif // MUSICPLAY_H
