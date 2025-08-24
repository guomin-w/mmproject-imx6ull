#include "musicplay.h"
#include "commonresource.h"
#include <QDebug>


MusicPlay::MusicPlay(const QString &music_fileName, QObject *parent) : QObject(parent)
{
    music_file = QString(MUSIC_PATH) + "/" + music_fileName;  // 音乐文件路径

    pcm_init();
    mixer_init();
}

int MusicPlay::channels = 2;
unsigned int MusicPlay::rate = 44100;
int MusicPlay::period_size = 1024;
int MusicPlay::periods = 16;

void MusicPlay::pcm_init()
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

void MusicPlay::mixer_init()
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

void MusicPlay::start()
{
    int ret;
    AVFormatContext *infmt_ctx = nullptr;           // 解封装上下文
    int audio_stream_idx = -1;                      // 音频流 id
    AVCodecContext *decodec_ctx = nullptr;          // 解码器上下文
    AVFrame *pframePCM = nullptr;                   // 目标 pcm 数据帧: 解码 + 重采样
    struct SwrContext *pcm_convert_ctx = nullptr;   // 重采样器上下文
    AVPacket *input_packet = nullptr;               // 数据包
    AVFrame *pframeSRC = nullptr;                   // 解码但未重采样的数据帧

    /* 打开音频文件, 创建解封装操作上下文 */
    ret = avformat_open_input(&infmt_ctx, music_file.toUtf8().constData(), nullptr, nullptr);
    if (ret != 0) {
        qWarning() << "avformat_open_input error" << endl;
        return;
    }
    /* 获取音频信息 */
    ret = avformat_find_stream_info(infmt_ctx, nullptr);
    if (ret < 0) {
        qWarning() << "avformat_find_stream_info error" << endl;
        avformat_close_input(&infmt_ctx);
        return;
    }
    /* 查找音频流 */
    for (unsigned int i = 0; i < infmt_ctx->nb_streams; i++) {
        if (infmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_stream_idx = i;
            break;
        }
    }
    if (audio_stream_idx == -1) {
        qWarning() << "can not find input audio stream" << endl;
        avformat_close_input(&infmt_ctx);
        return;
    }
    /* 获取流的编解码参数 */
    AVCodecParameters *codecpar = infmt_ctx->streams[audio_stream_idx]->codecpar;
    if (!codecpar) {
        qWarning() << "can not get AVCodecParameters" << endl;
        avformat_close_input(&infmt_ctx);
        return;
    }
    /* 获取解码器 */
    AVCodec* decodec = avcodec_find_decoder(codecpar->codec_id);
    if (!decodec) {
        qWarning() << "avcodec_find_decoder error" << endl;
        avformat_close_input(&infmt_ctx);
        return;
    }
    /* 创建解码器上下文 */
    decodec_ctx = avcodec_alloc_context3(decodec);
    if (!decodec_ctx) {
        qWarning() << "failed to allocate codec context" << endl;
        avformat_close_input(&infmt_ctx);
        return;
    }
    /* 复制流参数到解码器上下文 */
    ret = avcodec_parameters_to_context(decodec_ctx, codecpar);
    if (ret < 0) {
        qWarning() << "avcodec_parameters_to_context error" << endl;
        avcodec_free_context(&decodec_ctx);
        avformat_close_input(&infmt_ctx);
        return;
    }
    /* 打开解码器 */
    ret = avcodec_open2(decodec_ctx, decodec, nullptr);
    if (ret < 0) {
        qWarning() << "avcodec_open2 error" << endl;
        avcodec_free_context(&decodec_ctx);
        avformat_close_input(&infmt_ctx);
        return;
    }
    /* 创建和配置目标帧 */
    pframePCM = av_frame_alloc();
    pframePCM->format = AV_SAMPLE_FMT_S16;            // PCM 格式：16位有符号整型
    pframePCM->channel_layout = AV_CH_LAYOUT_STEREO;  // 声道布局：立体声
    pframePCM->sample_rate = rate;                    // 目标采样率（如44100）
    pframePCM->nb_samples = period_size;              // 一个周期的帧数（通常=ALSA周期大小）
    pframePCM->channels = channels;                   // 声道数（2）
    av_frame_get_buffer(pframePCM, 0);                // 分配内存缓冲区
    /* 重采样器配置 */
    pcm_convert_ctx = swr_alloc();
    swr_alloc_set_opts(
        pcm_convert_ctx,
        AV_CH_LAYOUT_STEREO,                                   // 输出声道布局
        AV_SAMPLE_FMT_S16,                                     // 输出格式（S16）
        pframePCM->sample_rate,                                // 输出采样率
        av_get_default_channel_layout(decodec_ctx->channels),  // 输入声道
        decodec_ctx->sample_fmt,                               // 输入格式（解码器输出格式）
        decodec_ctx->sample_rate,                              // 输入采样率
        0,                                                     // 日志标志
        nullptr                                                // 自定义矩阵
    );
    ret = swr_init(pcm_convert_ctx);
    if (ret < 0) {
        qWarning() << "swr_init error" << endl;
        av_frame_free(&pframePCM);
        avcodec_free_context(&decodec_ctx);
        avformat_close_input(&infmt_ctx);
        return;
    }
    /* 分配数据包: 解码前的压缩数据 */
    input_packet = av_packet_alloc();
    /* 分配数据帧: 解码但还未重采样的帧 */
    pframeSRC = av_frame_alloc();

    /* 获取音频总时长 (单位: 秒), 发送信号 */
    total_time = infmt_ctx->duration / AV_TIME_BASE;
    emit send_total_time(total_time);

    {   // {}作用是QMutexLocker与lock的作用范围, 确保其自动释放
        QMutexLocker locker(&mutex);
        is_stop = false;
        is_pause = false;
        need_seek = false;
    }

    /* 开始循环解码 */
    while (1) {
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
        /* 检查是否需要跳转 */
        {
            QMutexLocker locker(&mutex);
            if (need_seek) {
                locker.unlock();

                avcodec_flush_buffers(decodec_ctx);    // 清空解码器缓冲区
                int64_t target_ts = static_cast<int64_t>(seek_position / av_q2d(infmt_ctx->streams[audio_stream_idx]->time_base));  // 计算目标时间戳

                ret = av_seek_frame(infmt_ctx, audio_stream_idx, target_ts, AVSEEK_FLAG_BACKWARD);    // 执行跳转
                if (ret < 0) {
                    qWarning() << "Seek failed" << endl;
                }

                snd_pcm_drain(pcm);      // 清空缓冲区
                snd_pcm_prepare(pcm);    // 重新准备设备

                need_seek = false;
            }
        }
        /* 读取一个数据包 */
        ret = av_read_frame(infmt_ctx, input_packet);
        if (ret != 0) {
            if (ret == AVERROR_EOF) {       // 当前音乐已经播放完
                emit play_complete();       // 发送信号
                break;                      // 退出
            }
            else {
                qWarning() << "av_read_frame error" << endl;
                break;
            }
        }
        if (input_packet->stream_index == audio_stream_idx) {
            /* 向解码器发送一个数据包 */
            ret = avcodec_send_packet(decodec_ctx, input_packet);
            if (ret == AVERROR(EAGAIN)) {
                continue;
            }
            else if (ret < 0){
                qWarning() << "avcodec_send_packet error" << endl;
                break;
            }
            uint8_t *write_2_pcm = nullptr;  // 指向处理好的pcm数据
            while (1) {
                /* 获得解码后的数据帧 */
                ret = avcodec_receive_frame(decodec_ctx, pframeSRC);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                }
                else if (ret < 0) {
                    qWarning() << "avcodec_receive_frame error" << endl;
                    break;
                }

                /* 获取当前播放位置, 发送信号 */
                if (pframeSRC->pts != AV_NOPTS_VALUE) {  // 确保当前帧提供了有效的时间戳信息
                    curr_time = pframeSRC->pts * av_q2d(infmt_ctx->streams[audio_stream_idx]->time_base);
                    emit send_curr_time(curr_time);
                }

                /* 重采样 */
                int converted_data_num = swr_convert(pcm_convert_ctx, pframePCM->data, pframePCM->nb_samples,
                                                     (const uint8_t **)pframeSRC->extended_data, pframeSRC->nb_samples);
                write_2_pcm = pframePCM->data[0];
                /* 向 pcm 设备写入数据帧 */
                snd_pcm_writei(pcm, write_2_pcm, converted_data_num);
            }
        }
        /* 释放 input_packet 的引用 */
        av_packet_unref(input_packet);
    }

    /* 清理资源 */
    av_packet_free(&input_packet);
    if (pframeSRC) {
        av_frame_free(&pframeSRC);
    }
    if (pframePCM) {
        av_frame_free(&pframePCM);
    }
    if (pcm_convert_ctx) {
        swr_free(&pcm_convert_ctx);
    }
    if (decodec_ctx != nullptr){
        avcodec_close(decodec_ctx);
        avcodec_free_context(&decodec_ctx);
    }
    if (infmt_ctx != nullptr) {
        avformat_close_input(&infmt_ctx);
        avformat_free_context(infmt_ctx);
    }
    /* 停止、关闭 pcm 设备 */
    snd_pcm_drain(pcm);
    snd_pcm_close(pcm);
}


void MusicPlay::pause()
{
    QMutexLocker locker(&mutex);
    is_pause = true;
}


void MusicPlay::resume()
{
    QMutexLocker locker(&mutex);
    is_pause = false;
    cond.wakeAll();     // 唤醒所有等待线程
}


void MusicPlay::stop()
{
    QMutexLocker locker(&mutex);
    is_stop = true;
    cond.wakeAll();     // 唤醒线程，以确保退出
}


void MusicPlay::set_seek(int play_position)
{
    QMutexLocker locker(&mutex);
    need_seek = true;
    seek_position = play_position;
}


void MusicPlay::set_volume(int volume_percent)
{
    long minvol, maxvol;  // 最大、最小音量

    if (!volume_elem) {
        qWarning() << "get volume elem failed" << endl;
        return;
    }
    snd_mixer_selem_get_playback_volume_range(volume_elem, &minvol, &maxvol);  // 获取音量可设置范围 (0~127)
    snd_mixer_selem_set_playback_volume_all(volume_elem, (maxvol - minvol) * volume_percent / 100 + minvol);  // 设置音量
}
