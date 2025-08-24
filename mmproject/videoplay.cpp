#include "videoplay.h"
#include "commonresource.h"
#include <QDebug>
#include <QCoreApplication>


VideoPlay::VideoPlay(const QString &video_fileName, QObject *parent) : QObject(parent)
{
    video_file = QString(VIDEO_PATH) + "/" + video_fileName;    // 视频文件路径

    audio_queue = new VideoQueue();
    video_queue = new VideoQueue();
}

/* 不断获取数据包，发给相应的音频和视频处理线程 */
void VideoPlay::start()
{
    int ret;
    AVPacket *packet = nullptr;

    /* 打开视频文件, 创建解封装操作上下文 */
    ret = avformat_open_input(&infmt_ctx, video_file.toUtf8().constData(), nullptr, nullptr);
    if (ret != 0) {
        qWarning() << "avformat_open_input error";
        return;
    }
    /* 获取信息 */
    ret = avformat_find_stream_info(infmt_ctx, nullptr);
    if (ret < 0) {
        qWarning() << "avformat_find_stream_info error";
        avformat_close_input(&infmt_ctx);
        avformat_free_context(infmt_ctx);
        return;
    }
    /* 查找视频流和音频流 */
    for (unsigned int i = 0; i < infmt_ctx->nb_streams; i++) {
        if (infmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && video_stream_idx == -1) {
            video_stream_idx = i;
        }
        else if (infmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && audio_stream_idx == -1) {
            audio_stream_idx = i;
        }
    }
    if (video_stream_idx == -1 || audio_stream_idx == -1) {
        qWarning() << "can not find input video stream or audio stream";
        avformat_close_input(&infmt_ctx);
        avformat_free_context(infmt_ctx);
        return;
    }

    /* 启动视频、音频子线程 */
    if (!thread_audio) {
        thread_audio = new QThread(this);
        thread_video = new QThread(this);
        play_audio = new VideoPlayAudio(infmt_ctx, audio_stream_idx, audio_queue);  // 把上下文、流id、队列 传递给子线程
        play_video = new VideoPlayVideo(infmt_ctx, video_stream_idx, video_queue);
        play_audio->moveToThread(thread_audio);
        play_video->moveToThread(thread_video);
        connect(thread_audio, &QThread::started, play_audio, &VideoPlayAudio::start);
        connect(thread_video, &QThread::started, play_video, &VideoPlayVideo::start);
        connect(play_audio, &VideoPlayAudio::sig_curr_time, this, &VideoPlay::slot_curr_time);
        connect(play_audio, &VideoPlayAudio::sig_audio_clock, this, &VideoPlay::slot_audio_clock);
        connect(play_video, &VideoPlayVideo::sig_frame_ready, this, &VideoPlay::slot_ready_frame);
        connect(play_audio, &VideoPlayAudio::sig_audio_finished, this, &VideoPlay::slot_audio_finished);
        connect(play_video, &VideoPlayVideo::sig_video_finished, this, &VideoPlay::slot_video_finished);


        thread_video->start();
        thread_audio->start();
    }

    /* 获取总时长 (单位: 秒), 发送信号 */
    total_time = infmt_ctx->duration / AV_TIME_BASE;
    emit sig_total_time(total_time);

    {   // {}作用是QMutexLocker与lock的作用范围, 确保其自动释放
        QMutexLocker locker(&mutex);
        is_stop = false;
        is_pause = false;
    }

    /* 主循环：解封装, 数据包入队 */
    packet = av_packet_alloc();         // 分配数据包
    while (1) {
        {
            QMutexLocker locker(&mutex);
            if (is_pause) {
                cond.wait(&mutex);      // 释放锁并等待条件变量
            }
        }
        {
            QMutexLocker locker(&mutex);
            if (is_stop) {
                break;
            }
        }
        /* 读取一个数据包 */
        ret = av_read_frame(infmt_ctx, packet);
        if (ret != 0) {
            if (ret == AVERROR_EOF) {
                /* 读到文件末尾，入队一个特殊的数据包 */
                packet->data = nullptr;
                packet->size = 0;
                video_queue->queue_push(packet);
                audio_queue->queue_push(packet);
                break;
            }
            else {
                qWarning() << "av_read_frame error";
                break;
            }
        }
        /* 视频帧队列 入队 */
        if (packet->stream_index == video_stream_idx) {
            video_queue->queue_push(packet);
        }
        /* 音频帧队列 入队 */
        else if (packet->stream_index == audio_stream_idx) {
            audio_queue->queue_push(packet);
        }
        else {
            av_packet_unref(packet);
        }
        packet = av_packet_alloc();    // packet的解引用在相应的子线程中进行，这里每次都需新建 packet

        QCoreApplication::processEvents();  // 处理事件
    }    

    /* 循环等待视频和音频线程完成 */
    while (1) {
        /* 若是因为 is_stop 退出，则入队一个停止数据包 */
        {
            QMutexLocker locker(&mutex);
            if (is_stop) {
                AVPacket* pkt_stop = av_packet_alloc();
                pkt_stop->data = nullptr;
                pkt_stop->size = 0;
                video_queue->queue_push(pkt_stop);
                audio_queue->queue_push(pkt_stop);

                play_audio->stop();
                play_video->stop();

                break;
            }
        }
        {
            QMutexLocker locker(&mutex);
            if (audio_finished && video_finished) {
                emit sig_play_complete();    // 发送播放完成信号
                break;
            }
        }

        QCoreApplication::processEvents();  // 处理事件
    }

    /* 关闭所有子线程 */
    if (thread_audio) {
        play_audio->stop();
        play_video->stop();
        thread_audio->quit();
        thread_audio->wait();
        thread_video->quit();
        thread_video->wait();
        delete play_audio;
        play_audio = nullptr;
        delete play_video;
        play_video = nullptr;
        delete thread_audio;
        thread_audio = nullptr;
        delete thread_video;
        thread_video = nullptr;
    }

    /* 清理资源 */
    if (packet) {
        av_packet_free(&packet);
        packet = nullptr;
    }
    if (infmt_ctx) {
        avformat_close_input(&infmt_ctx);
        avformat_free_context(infmt_ctx);
        infmt_ctx = nullptr;
    }
    /* 清除队列 */
    if (audio_queue) {
        audio_queue->queue_flush();
        video_queue->queue_flush();
    }
}

void VideoPlay::pause()
{
    {
        QMutexLocker locker(&mutex);
        is_pause = true;
    }
    if (play_audio) {
        play_audio->pause();
        play_video->pause();
    }
}

void VideoPlay::resume()
{
    {
        QMutexLocker locker(&mutex);
        is_pause = false;
        cond.wakeAll();     // 唤醒所有等待线程
    }
    if (play_audio) {
        play_audio->resume();
        play_video->resume();
    }
}

void VideoPlay::stop()
{
    QMutexLocker locker(&mutex);
    is_stop = true;
    cond.wakeAll();     // 唤醒所有等待线程
}

void VideoPlay::set_volume(int volume_percent)
{
    /* 调用音频线程的音量设置函数 */
    if (play_audio) {
        play_audio->set_volume(volume_percent);
    }
}

void VideoPlay::slot_curr_time(int curr_time)
{
    /* 向上层发送信号 */
    emit sig_curr_time(curr_time);
}

void VideoPlay::slot_audio_clock(double audio_clock)
{
    /* 调用视频线程的同步函数 */
    if (play_video) {
        play_video->set_clock(audio_clock);
    }
}

void VideoPlay::slot_ready_frame(int linesize)
{
    emit sig_ready_frame(linesize);
}

void VideoPlay::slot_audio_finished()
{
    QMutexLocker locker(&mutex);
    audio_finished = true;
}

void VideoPlay::slot_video_finished()
{
    QMutexLocker locker(&mutex);
    video_finished = true;
}
