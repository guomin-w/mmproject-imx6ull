#include "videoplayvideo.h"
#include <QDebug>


VideoPlayVideo::VideoPlayVideo(AVFormatContext *infmt_ctx, int video_stream_idx, VideoQueue *video_queue, QObject *parent) : QObject(parent)
{
    this->infmt_ctx = infmt_ctx;
    this->video_stream_idx = video_stream_idx;
    this->video_queue = video_queue;
}

void VideoPlayVideo::start()
{
    int ret;
    AVCodecContext *video_dec_ctx = nullptr;
    struct SwsContext *sws_ctx = nullptr;
    AVFrame *rgb_frame_src = nullptr;
    AVFrame *rgb_frame_tar = nullptr;
    AVPacket *pkt = nullptr;

    double video_pts;   // 视频帧 pts
    double sync_diff;   // 时钟差

    av_log_set_level(AV_LOG_FATAL); // 只显示致命错误

    /* 初始化、打开视频流解码器 */
    AVCodecParameters *video_codecpar = infmt_ctx->streams[video_stream_idx]->codecpar;
    if (!video_codecpar) {
        qWarning() << "can not get video_codecpar AVCodecParameters" << endl;
        return;
    }
    AVCodec *video_codec = avcodec_find_decoder(video_codecpar->codec_id);
    if (!video_codec) {
        qWarning() << "video: avcodec_find_decoder error" << endl;
        return;
    }
    video_dec_ctx = avcodec_alloc_context3(video_codec);
    if (!video_dec_ctx) {
        qWarning() << "failed to allocate video codec context" << endl;
        return;
    }

    avcodec_parameters_to_context(video_dec_ctx, video_codecpar);

    video_dec_ctx->max_b_frames = 0;    // 禁用B帧
    video_dec_ctx->skip_frame = AVDISCARD_NONREF;    // 跳过非参考帧
    video_dec_ctx->skip_loop_filter = AVDISCARD_ALL;  // 跳过环路滤波
    video_dec_ctx->skip_idct = AVDISCARD_NONREF;      // 跳过IDCT
    video_dec_ctx->error_concealment = FF_EC_GUESS_MVS | FF_EC_DEBLOCK;
    video_dec_ctx->flags2 |= AV_CODEC_FLAG2_FAST;     // 启用快速解码
    video_dec_ctx->flags |= AV_CODEC_FLAG_LOW_DELAY;

    ret = avcodec_open2(video_dec_ctx, video_codec, nullptr);
    if (ret < 0) {
        qWarning() << "video_codec: avcodec_open2 error" << endl;
        avcodec_close(video_dec_ctx);
        avcodec_free_context(&video_dec_ctx);
        return;
    }
    /* 初始化视频缩放器 */
    rgb_frame_tar = av_frame_alloc();
    if (!rgb_frame_tar) {
        qWarning() << "Failed to allocate RGB frame" << endl;
        avcodec_close(video_dec_ctx);
        avcodec_free_context(&video_dec_ctx);
        return;
    }
    rgb_frame_tar->format = AV_PIX_FMT_RGB24;           // 24位RGB格式
    rgb_frame_tar->width  = 200;                // 宽度
    rgb_frame_tar->height = 150;                // 高度
    ret = av_frame_get_buffer(rgb_frame_tar, 0);        // 分配内存缓冲区
    if (ret < 0) {
        qWarning() << "Failed to allocate RGB frame buffer" << endl;
        avcodec_close(video_dec_ctx);
        avcodec_free_context(&video_dec_ctx);
        av_frame_free(&rgb_frame_tar);
        return;
    }
    sws_ctx = sws_getContext(
        video_dec_ctx->width,      // 输入宽度
        video_dec_ctx->height,     // 输入高度
        video_dec_ctx->pix_fmt,    // 输入格式（如YUV420P）
        rgb_frame_tar->width,      // 输出宽度
        rgb_frame_tar->height,     // 输出高度
        AV_PIX_FMT_RGB24,          // 输出格式（RGB24）
        SWS_FAST_BILINEAR,  // 使用快速双线性
        nullptr, nullptr, nullptr
    );
    if (!sws_ctx) {
        qWarning() << "Failed to create SwsContext" << endl;
        avcodec_close(video_dec_ctx);
        avcodec_free_context(&video_dec_ctx);
        av_frame_free(&rgb_frame_tar);
        return;
    }

    /* 分配数据帧: 解码但还未重采样的帧 */
    rgb_frame_src = av_frame_alloc();

    {   // {}作用是QMutexLocker与lock的作用范围, 确保其自动释放
        QMutexLocker locker(&mutex);
        is_stop = false;
        is_pause = false;
    }

    /* 初始化共享内存 */
    QSharedMemory sharedMem("VideoFrameBuffer");
    int frameSize = rgb_frame_tar->linesize[0] * rgb_frame_tar->height;
    if (!sharedMem.create(frameSize)) {
        if (sharedMem.error() == QSharedMemory::AlreadyExists) {
            sharedMem.attach();
        } else {
            qWarning() << "Failed to create shared memory";
            avcodec_close(video_dec_ctx);
            avcodec_free_context(&video_dec_ctx);
            av_frame_free(&rgb_frame_tar);
            return;
        }
    }

    /* 循环解码 */
    while (1) {
        /* 先检查是否暂停 (它会阻塞等待)，再检查是否退出 */
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
        /* 处理视频数据包 */
        pkt = av_packet_alloc();
        if (video_queue->queue_pop(pkt)) {    // 从视频队列中取数据包
            if (pkt->data == nullptr && pkt->size == 0) {    // 读取到特殊数据包
                emit sig_video_finished();    // 通知一级线程已完成
                break;
            }
            /* 音视频同步, 以音频为基准 */
            {
                QMutexLocker locker(&mutex);
                if (audio_clock != 0.0) {
                    locker.unlock();
                    video_pts = pkt->pts * av_q2d(infmt_ctx->streams[video_stream_idx]->time_base);    // 视频当前PTS（秒）
                    sync_diff = video_pts - audio_clock;
                    if (sync_diff > 0.1) {            // 视频比音频快，延迟显示
                        QThread::usleep((unsigned long)(sync_diff * 1000000));
                    } else if (sync_diff < -0.1) {    // 视频比音频慢，跳过帧
                        continue;
                    }
                }
            }
            /* 向视频解码器发送一个数据包 */
            ret = avcodec_send_packet(video_dec_ctx, pkt);
            if (ret == AVERROR(EAGAIN)) {
                continue;
            }
            else if (ret < 0){
                qWarning() << "video: avcodec_send_packet error" << endl;
                break;
            }
            /* 获得解码后的视频帧 */
            while (1) {
                ret = avcodec_receive_frame(video_dec_ctx, rgb_frame_src);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                }
                else if (ret < 0) {
                    qWarning() << "video: avcodec_receive_frame error" << endl;
                    break;
                }
                /* 缩放视频帧 */
                sws_scale(sws_ctx,
                          rgb_frame_src->data,
                          rgb_frame_src->linesize,
                          0,
                          rgb_frame_src->height,
                          rgb_frame_tar->data,
                          rgb_frame_tar->linesize);
                /* 写入共享内存 */
                if (sharedMem.lock()) {
                    memcpy(sharedMem.data(), rgb_frame_tar->data[0], frameSize);
                    sharedMem.unlock();
                    // 发送信号（附带linesize信息）
                    emit sig_frame_ready(rgb_frame_tar->linesize[0]);
                }
            }
            av_packet_unref(pkt);
        }
    }

    /* 清理资源 */
    if (pkt) {
        av_packet_free(&pkt);
    }
    if (rgb_frame_tar) {
        av_frame_free(&rgb_frame_tar);
    }
    if (rgb_frame_src) {
        av_frame_free(&rgb_frame_src);
    }
    if (sws_ctx) {
        sws_freeContext(sws_ctx);
    }
    if (video_dec_ctx) {
        avcodec_close(video_dec_ctx);
        avcodec_free_context(&video_dec_ctx);
    }

    if (sharedMem.isAttached()) {
        sharedMem.detach();
    }
}

void VideoPlayVideo::pause()
{
    QMutexLocker locker(&mutex);
    is_pause = true;
}

void VideoPlayVideo::resume()
{
    QMutexLocker locker(&mutex);
    is_pause = false;
    cond.wakeAll();     // 唤醒所有等待线程
}

void VideoPlayVideo::stop()
{
    QMutexLocker locker(&mutex);
    is_stop = true;
    cond.wakeAll();     // 唤醒线程，以确保退出
}

void VideoPlayVideo::set_clock(double sync_clock)
{
    QMutexLocker locker(&mutex);
    audio_clock = sync_clock;
}
