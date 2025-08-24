#include "videoplayaudio.h"
#include <QDebug>


VideoPlayAudio::VideoPlayAudio(AVFormatContext *infmt_ctx, int audio_stream_idx, VideoQueue *audio_queue, QObject *parent) : QObject(parent)
{
    this->infmt_ctx = infmt_ctx;
    this->audio_stream_idx = audio_stream_idx;
    this->audio_queue = audio_queue;

    pcm_init();
    mixer_init();
}

int VideoPlayAudio::channels = 2;
unsigned int VideoPlayAudio::rate = 44100;
int VideoPlayAudio::period_size = 1024;
int VideoPlayAudio::periods = 16;

void VideoPlayAudio::pcm_init()
{
    int ret;
    snd_pcm_hw_params_t *hwparams = nullptr;  // 描述PCM设备的硬件配置参数

    if (!pcm) {
        /* 打开 PCM 设备 */
        ret = snd_pcm_open(&pcm, PCM_DEVICE, SND_PCM_STREAM_PLAYBACK, 0);
        if (ret < 0) {
            qWarning() << "snd_pcm_open failed" << endl;
            return;
        }
        /* 实例化 hwparams 对象 */
        snd_pcm_hw_params_malloc(&hwparams);
        /* 获取PCM设备当前硬件配置, 对hwparams进行设置 */
        ret = snd_pcm_hw_params_any(pcm, hwparams);
        if (ret < 0) {
            qWarning() << "snd_pcm_hw_params_any failed" << endl;
            goto err2;
        }
        /* 设置访问类型: SND_PCM_ACCESS_RW_INTERLEAVED 交错访问模式 */
        ret = snd_pcm_hw_params_set_access(pcm, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED);
        if (ret < 0) {
            qWarning() << "snd_pcm_hw_params_set_access failed" << endl;
            goto err2;
        }
        /* 设置数据格式: 有符号16位、小端模式 */
        ret = snd_pcm_hw_params_set_format(pcm, hwparams, SND_PCM_FORMAT_S16_LE);
        if (ret < 0) {
            qWarning() << "snd_pcm_hw_params_set_format failed" << endl;
            goto err2;
        }
        /* 设置声道数量 */
        ret = snd_pcm_hw_params_set_channels(pcm, hwparams, channels);
        if (ret < 0) {
            qWarning() << "snd_pcm_hw_params_set_channels failed" << endl;
            goto err2;
        }
        /* 设置采样率大小 */
        ret = snd_pcm_hw_params_set_rate(pcm, hwparams, rate, 0);
        if (ret < 0) {
            qWarning() << "snd_pcm_hw_params_set_rate failed" << endl;
            goto err2;
        }
        /* 设置周期大小: 一个周期的帧数 */
        ret = snd_pcm_hw_params_set_period_size(pcm, hwparams, period_size, 0);
        if (ret < 0) {
            qWarning() << "snd_pcm_hw_params_set_period_size failed" << endl;
            goto err2;
        }
        /* 设置周期数: 驱动层的 buffer 大小 */
        ret = snd_pcm_hw_params_set_periods(pcm, hwparams, periods, 0);
        if (ret < 0) {
            qWarning() << "snd_pcm_hw_params_set_periods failed" << endl;
            goto err2;
        }
        /* 安装/加载硬件配置参数 */
        ret = snd_pcm_hw_params(pcm, hwparams);  // 其内部会自动调用snd_pcm_prepare()函数，PCM设备的状态被更改为SND_PCM_STATE_PREPARED，即准备好的状态
        snd_pcm_hw_params_free(hwparams);  // 释放 hwparams 对象占用的内存
        if (ret < 0) {
            qWarning() << "snd_pcm_hw_params failed" << endl;
            goto err1;
        }
    }
    snd_pcm_prepare(pcm);   // 将设备置为就绪状态
    return;

err2:
    snd_pcm_hw_params_free(hwparams);  // 释放 hwparams 对象占用的内存
err1:
    snd_pcm_close(pcm);     /* 关闭 PCM 设备 */
    pcm = nullptr;          /* 关闭后，将其置为NULL */
}

void VideoPlayAudio::mixer_init()
{
    snd_mixer_t *mixer = nullptr;    // 混音器

    if (!volume_elem) {
        /* 初始化 mixer 设备 */
        int ret;
        snd_mixer_elem_t *elem = nullptr;
        const char *elem_name;
        /* 打开混音器 */
        ret = snd_mixer_open(&mixer, 0);
        if (ret < 0) {
            qWarning() << "snd_mixer_open error" << endl;
            return;
        }
        /* 关联设备 */
        ret = snd_mixer_attach(mixer, MIXER_DEVICE);
        if (ret < 0) {
            qWarning() << "snd_mixer_attach error" << endl;
            snd_mixer_close(mixer);
            return;
        }
        /* 注册混音器 */
        ret = snd_mixer_selem_register(mixer, nullptr, nullptr);
        if (ret < 0) {
            qWarning() << "snd_mixer_selem_register error" << endl;
            snd_mixer_close(mixer);
            return;
        }
        /* 加载混音器 */
        ret = snd_mixer_load(mixer);
        if (ret < 0) {
            qWarning() << "snd_mixer_load error" << endl;
            snd_mixer_close(mixer);
            return;
        }
        /* 遍历查找混音器中的元素 */
        elem = snd_mixer_first_elem(mixer);  // 第一个元素
        while (elem) {
            elem_name = snd_mixer_selem_get_name(elem);  // 获取元素的名称
            if (strcmp(elem_name, "Headphone Playback ZC") == 0) {      // 控制耳机的开启
                if (snd_mixer_selem_has_playback_switch(elem)) {        // 有开关属性
                    snd_mixer_selem_set_playback_switch_all(elem, 1);   // 开启
                }
            }
            else if (strcmp(elem_name, "Headphone") == 0) {         // 耳机音量
                if (snd_mixer_selem_has_playback_volume(elem)) {    // 有音量属性
                    volume_elem = elem;                             // 找到了耳机的音量元素
                }
            }

            elem = snd_mixer_elem_next(elem);  // 下一个元素
        }
    }
}

void VideoPlayAudio::start()
{
    int ret;

    AVCodecContext *audio_dec_ctx = nullptr;
    struct SwrContext *swr_ctx = nullptr;
    AVFrame *pcm_frame_src = nullptr;
    AVFrame *pcm_frame_tar = nullptr;
    AVPacket *pkt = nullptr;

    /* 初始化、打开音频流解码器 */
    AVCodecParameters *audio_codecpar = infmt_ctx->streams[audio_stream_idx]->codecpar;
    if (!audio_codecpar) {
        qWarning() << "can not get audio_codecpar AVCodecParameters" << endl;
        return;
    }
    AVCodec *audio_codec = avcodec_find_decoder(audio_codecpar->codec_id);
    if (!audio_codec) {
        qWarning() << "sudio: avcodec_find_decoder error" << endl;
        return;
    }
    audio_dec_ctx = avcodec_alloc_context3(audio_codec);
    if (!audio_dec_ctx) {
        qWarning() << "failed to allocate audio codec context" << endl;
        return;
    }
    avcodec_parameters_to_context(audio_dec_ctx, audio_codecpar);
    ret = avcodec_open2(audio_dec_ctx, audio_codec, nullptr);
    if (ret < 0) {
        qWarning() << "audio_codec: avcodec_open2 error" << endl;
        avcodec_close(audio_dec_ctx);
        avcodec_free_context(&audio_dec_ctx);
        return;
    }
    /* 初始化音频重采样器 */
    pcm_frame_tar = av_frame_alloc();
    if (!pcm_frame_tar) {
        qWarning() << "Failed to allocate PCM frame" << endl;
        avcodec_close(audio_dec_ctx);
        avcodec_free_context(&audio_dec_ctx);
        return;
    }
    pcm_frame_tar->format = AV_SAMPLE_FMT_S16;            // PCM 格式：16位有符号整型
    pcm_frame_tar->channel_layout = AV_CH_LAYOUT_STEREO;  // 声道布局：立体声
    pcm_frame_tar->sample_rate = rate;                    // 目标采样率（如44100）
    pcm_frame_tar->nb_samples = period_size;              // 一个周期的帧数（通常=ALSA周期大小）
    pcm_frame_tar->channels = channels;                   // 声道数（2）
    ret = av_frame_get_buffer(pcm_frame_tar, 0);          // 分配内存缓冲区
    if (ret < 0) {
        avcodec_close(audio_dec_ctx);
        avcodec_free_context(&audio_dec_ctx);
        av_frame_free(&pcm_frame_tar);
        qWarning() << "Failed to allocate PCM frame buffer" << endl;
        return;
    }
    /* 重采样器配置 */
    swr_ctx = swr_alloc();
    swr_alloc_set_opts(
        swr_ctx,
        AV_CH_LAYOUT_STEREO,                                   // 输出声道布局
        AV_SAMPLE_FMT_S16,                                     // 输出格式（S16）
        pcm_frame_tar->sample_rate,                            // 输出采样率
        av_get_default_channel_layout(audio_dec_ctx->channels),  // 输入声道
        audio_dec_ctx->sample_fmt,                               // 输入格式（解码器输出格式）
        audio_dec_ctx->sample_rate,                              // 输入采样率
        0,                                                     // 日志标志
        nullptr                                                // 自定义矩阵
    );
    ret = swr_init(swr_ctx);
    if (ret < 0) {
        qWarning() << "swr_init error" << endl;
        av_frame_free(&pcm_frame_tar);
        avcodec_free_context(&audio_dec_ctx);
        avformat_close_input(&infmt_ctx);
        return;
    }

    /* 分配数据帧: 解码但还未重采样的帧 */
    pcm_frame_src = av_frame_alloc();

    {   // {}作用是QMutexLocker与lock的作用范围, 确保其自动释放
        QMutexLocker locker(&mutex);
        is_stop = false;
        is_pause = false;
    }

    /* 循环播放音频 */
    while (1) {
        /* 先检查是否暂停 (它会阻塞等待)，再检查是否退出 */
        {
            QMutexLocker locker(&mutex);
            if (is_pause) {
                snd_pcm_drain(pcm);     // 停止设备, 处理完缓冲区数据再停止
                cond.wait(&mutex);      // 释放锁并等待条件变量
                snd_pcm_prepare(pcm);   // 将设备置为就绪状态
            }
        }
        {
            QMutexLocker locker(&mutex);
            if (is_stop) {
                break;
            }
        }
        /* 处理音频数据包 */
        pkt = av_packet_alloc();
        if (audio_queue->queue_pop(pkt)) {    // 从音频队列中获取数据包
            if (pkt->data == nullptr && pkt->size == 0) {    // 读取到特殊数据包
                emit sig_audio_finished();    // 通知一级线程已完成
                break;
            }
            /* 向视频解码器发送一个数据包 */
            ret = avcodec_send_packet(audio_dec_ctx, pkt);
            if (ret == AVERROR(EAGAIN)) {
                continue;
            } else if (ret < 0){
                qWarning() << "audio: avcodec_send_packet error" << endl;
                break;
            }
            /* 获得解码后的音频帧 */
            while (1) {
                ret = avcodec_receive_frame(audio_dec_ctx, pcm_frame_src);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                }
                else if (ret < 0) {
                    qWarning() << "audio: avcodec_receive_frame error" << endl;
                    break;
                }
                /* 更新音频时钟, 发送信号 */
                if (pcm_frame_src->pts != AV_NOPTS_VALUE) {    // 确保当前帧提供了有效的时间戳信息
                    audio_clock = pcm_frame_src->pts * av_q2d(infmt_ctx->streams[audio_stream_idx]->time_base);
                    emit sig_audio_clock(audio_clock);
                }
                /* 发送当前播放位置, 只取整数 */
                curr_time = (int)audio_clock;
                emit sig_curr_time(curr_time);
                /* 音频重采样 */
                int converted_data_num = swr_convert(swr_ctx,
                                                     pcm_frame_tar->data,
                                                     pcm_frame_tar->nb_samples,
                                                     (const uint8_t **)pcm_frame_src->extended_data,
                                                     pcm_frame_src->nb_samples);
                /* 向 pcm 设备写入音频帧 */
                if (converted_data_num > 0) {
                    snd_pcm_writei(pcm, pcm_frame_tar->data[0], converted_data_num);
                }
            }
            /* 释放 packet 的引用 */
            av_packet_unref(pkt);
        }
    }

    /* 清理资源 */
    if (pkt) {
        av_packet_free(&pkt);
    }
    if (pcm_frame_tar) {
        av_frame_free(&pcm_frame_tar);
    }
    if (pcm_frame_src) {
        av_frame_free(&pcm_frame_src);
    }
    if (swr_ctx) {
        swr_free(&swr_ctx);
    }
    if (audio_dec_ctx) {
        avcodec_close(audio_dec_ctx);
        avcodec_free_context(&audio_dec_ctx);
    }
    /* 停止、关闭 pcm 设备 */
    snd_pcm_drain(pcm);
    snd_pcm_close(pcm);
}

void VideoPlayAudio::pause()
{
    QMutexLocker locker(&mutex);
    is_pause = true;
}

void VideoPlayAudio::resume()
{
    QMutexLocker locker(&mutex);
    is_pause = false;
    cond.wakeAll();     // 唤醒所有等待线程
}

void VideoPlayAudio::stop()
{
    QMutexLocker locker(&mutex);
    is_stop = true;
    cond.wakeAll();     // 唤醒线程，以确保退出
}

void VideoPlayAudio::set_volume(int volume_percent)
{
    long minvol, maxvol;  // 最大、最小音量

    if (!volume_elem) {
        qWarning() << "get volume elem failed" << endl;
        return;
    }
    snd_mixer_selem_get_playback_volume_range(volume_elem, &minvol, &maxvol);  // 获取音量可设置范围 (0~127)
    snd_mixer_selem_set_playback_volume_all(volume_elem, (maxvol - minvol) * volume_percent / 100 + minvol);  // 设置音量
}
