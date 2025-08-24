#include "videowidget.h"
#include "commonresource.h"
#include <QDebug>
#include <QDir>


VideoWidget::VideoWidget(QWidget *parent) : QWidget(parent), m_sharedMem("VideoFrameBuffer")
{
    this->resize(1024, 600);

    btn_back = new QPushButton(this);
    btn_back->setText("返回");
    btn_back->setGeometry(20, 20, 100, 40);

    label_list = new QLabel(this);
    label_list->setGeometry(20, 80, 120, 40);
    label_list->setText(" 视频文件列表:");
    label_list->setObjectName("label_list");

    list_videos = new QListWidget(this);
    list_videos->setObjectName("list_videos");
    list_videos->setGeometry(20, 120, 280, 360);

    label_video = new QLabel(this);
    label_video->setObjectName("label_video");
    label_video->setGeometry(340, 40, 600, 450);

    slider_video = new QSlider(Qt::Horizontal, this);
    slider_video->setObjectName("slider_video");
    slider_video->setGeometry(20, 500, 984, 20);

    btn_play = new QPushButton(this);
    btn_play->setObjectName("btn_play_video");
    btn_play->setGeometry(20, 540, 40, 40);
    btn_play->setCheckable(true);

    btn_next = new QPushButton(this);
    btn_next->setObjectName("btn_next_video");
    btn_next->setGeometry(160, 540, 40, 40);

    label_curr_time = new QLabel(this);
    label_curr_time->setGeometry(300, 540, 60, 40);
    label_curr_time->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    label_curr_time->setText("00:00");

    label_total_time = new QLabel(this);
    label_total_time->setGeometry(360, 540, 80, 40);
    label_total_time->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    label_total_time->setText(" / --:--");

    QFont font;
    font.setPixelSize(20);
    label_curr_time->setFont(font);
    label_total_time->setFont(font);

    btn_volume = new QPushButton(this);
    btn_volume->setObjectName("btn_volume_video");
    btn_volume->setGeometry(804, 540, 40, 40);

    slider_volume = new QSlider(this);
    slider_volume->setObjectName("slider_volume_video");
    slider_volume->setRange(0, 100);
    slider_volume->setValue(50);
    slider_volume->setGeometry(770, 360, 20, 200);
    slider_volume->hide();    // 默认隐藏

    btn_scale = new QPushButton(this);
    btn_scale->setObjectName("btn_scale");
    btn_scale->setGeometry(944, 540, 40, 40);
    btn_scale->setCheckable(true);

    connect(btn_back, &QPushButton::clicked, this, &VideoWidget::btnBackClicked);
    connect(btn_play, &QPushButton::clicked, this, &VideoWidget::btnPlayClicked);
    connect(btn_next, &QPushButton::clicked, this, &VideoWidget::btnNextClicked);
    connect(btn_volume, &QPushButton::clicked, this, &VideoWidget::btnVolumeClicked);
    connect(btn_scale, &QPushButton::clicked, this, &VideoWidget::btnScaleClicked);
    connect(list_videos, &QListWidget::itemClicked, this, &VideoWidget::listVideosCliked);
    connect(slider_volume, &QSlider::sliderReleased, this, &VideoWidget::sliderVolumeReleased);

    scanVideo();
}

void VideoWidget::btnBackClicked()
{
    stop_play_thread();    // 关闭播放线程

    parentWidget()->show();
    this->close();
}

void VideoWidget::scanVideo()
{
    QDir dir(VIDEO_PATH);

    QStringList filters;    // 定义过滤器
    filters << "*.mp4" << "*.m4v" << "*.mkv";

    /* 获取目录下所有符合条件的文件 */
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files, QDir::Name);    // 按名字排序
    for (int i = 0; i < files.count(); i++) {
        video_files.append(files.at(i).fileName());
        QString list_item = QString::fromUtf8(files.at(i).fileName().toUtf8().data());
        list_videos->addItem(list_item);    // 添加到 listWidget
    }

    video_cnt = video_files.size();
    curr_indx = 0;
}

void VideoWidget::create_play_thread()
{
    if (!video_play) {
        thread = new QThread(this);
        video_play = new VideoPlay(video_files[curr_indx]);
        video_play->moveToThread(thread);

        connect(thread, &QThread::started, video_play, &VideoPlay::start);
        connect(video_play, &VideoPlay::sig_total_time, this, &VideoWidget::setTotalTime);
        connect(video_play, &VideoPlay::sig_curr_time, this, &VideoWidget::setCurrTime);
        connect(video_play, &VideoPlay::sig_play_complete, this, &VideoWidget::playComplete);
        connect(video_play, &VideoPlay::sig_ready_frame, this, &VideoWidget::displayFrame);

        list_videos->setCurrentRow(curr_indx);    // 刷新列表 list_videos
    }
}

void VideoWidget::stop_play_thread()
{
    if (video_play) {
        video_play->stop();

        thread->quit();
        thread->wait();

        delete video_play;
        video_play = nullptr;

        delete thread;
        thread = nullptr;
    }
}

void VideoWidget::btnPlayClicked()
{
    bool isChecked = btn_play->isChecked();  // 获取当前状态

    if (isChecked) {
        if (!video_play) {
            create_play_thread();   // 创建线程
        }
        if (!thread->isRunning()) {
            thread->start();        // 启动线程
        }
        else {
            video_play->resume();   // 恢复线程
        }
    }
    else {
        if (video_play) {
            video_play->pause();    // 暂停线程
        }
    }
}

void VideoWidget::btnNextClicked()
{
    stop_play_thread();

    curr_indx++;
    if (curr_indx >= video_cnt) {
        curr_indx = 0;
    }

    create_play_thread();

    if (btn_play->isChecked()) {
        thread->start();
    }
}

void VideoWidget::btnScaleClicked()
{

}

void VideoWidget::btnVolumeClicked()
{
    if (slider_volume->isVisible()) {
        slider_volume->setVisible(false);
    } else {
        slider_volume->setVisible(true);
    }
}

void VideoWidget::sliderVolumeReleased()
{
    if (video_play) {
        video_play->set_volume(slider_volume->value());
        int volume = slider_volume->value();
        qDebug() << "set volume: " << volume << endl;
    }
}

void VideoWidget::listVideosCliked(QListWidgetItem *item)
{
    stop_play_thread();
    curr_indx = list_videos->row(item);
    create_play_thread();
    if (btn_play->isChecked()) {
        thread->start();
    }
}

void VideoWidget::setCurrTime(int curr_time)
{
    /* 设置标签 */
    int minute = curr_time / 60;
    int second = curr_time % 60;

    QString video_position;
    video_position.clear();

    if (minute >= 10) {
        video_position = QString::number(minute, 10);
    } else {
        video_position = "0" + QString::number(minute, 10);
    }

    if (second >= 10) {
        video_position = video_position + ":" + QString::number(second, 10);
    } else {
        video_position = video_position + ":0" + QString::number(second, 10);
    }

    label_curr_time->setText(video_position);

    /* 刷新进度条 */
    if (!slider_video->isSliderDown()) {
        slider_video->setValue(curr_time);
    }
}

void VideoWidget::setTotalTime(int total_time)
{
    /* 设置标签 */
    int minute = total_time / 60;
    int second = total_time % 60;

    QString video_duration;
    video_duration.clear();

    if (minute >= 10) {
        video_duration = QString::number(minute, 10);  // 把整数转为字符串，以十进制形式
    } else {
        video_duration = "0" + QString::number(minute, 10);
    }

    if (second >= 10) {
        video_duration = " / " + video_duration + ":" + QString::number(second, 10);
    } else {
        video_duration = video_duration + ":0" + QString::number(second, 10);
    }

    label_total_time->setText(video_duration);

    /* 设置进度条范围 */
    slider_video->setRange(0, total_time);
}

void VideoWidget::playComplete()
{
    stop_play_thread();

    curr_indx++;
    if (curr_indx >= video_cnt) {
        curr_indx = 0;
    }

    create_play_thread();

    btn_play->setChecked(false);    // 停止状态
}

void VideoWidget::displayFrame(int linesize)
{
    if (!m_sharedMem.isAttached()) {
        /* 尝试附加共享内存 */
        if (!m_sharedMem.attach()) {
            return;
        }
    }

    if (m_sharedMem.lock()) {
        /* 使用解码线程传来的linesize创建QImage */
        QImage img(
            reinterpret_cast<const uchar*>(m_sharedMem.constData()),
            200, 150,   // 固定尺寸
            linesize,   // 关键：使用实际的linesize
            QImage::Format_RGB888
        );

        m_sharedMem.unlock();

        if (!img.isNull()) {
            QPixmap pixmap = QPixmap::fromImage(img);
            /* 图片缩放 */
            if (pixmap.size() != label_video->size()) {
                pixmap = pixmap.scaled(label_video->size(), Qt::IgnoreAspectRatio, Qt::FastTransformation);
            }
            label_video->setPixmap(pixmap);
        }
    }
}

