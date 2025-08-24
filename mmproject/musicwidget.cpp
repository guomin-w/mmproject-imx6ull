#include "musicwidget.h"
#include "commonresource.h"
#include <QDir>
#include <QRandomGenerator>
#include <QDebug>


MusicWidget::MusicWidget(QWidget *parent) : QWidget(parent)
{
    this->resize(1024, 600);

    btn_back = new QPushButton(this);
    btn_back->setText("返回");
    btn_back->setGeometry(20, 20, 100, 40);

    list_musics = new QListWidget(this);
    list_musics->setObjectName("list_musics");
    list_musics->setGeometry(56, 80, 400, 400);

    btn_prev = new QPushButton(this);
    btn_prev->setObjectName("btn_prev_music");
    btn_prev->setGeometry(50, 495, 80, 80);

    btn_play = new QPushButton(this);
    btn_play->setCheckable(true);
    btn_play->setObjectName("btn_play_music");
    btn_play->setGeometry(216, 495, 80, 80);

    btn_next = new QPushButton(this);
    btn_next->setObjectName("btn_next_music");
    btn_next->setGeometry(382, 495, 80, 80);

    label_cd = new QLabel(this);
    label_cd->setObjectName("label_cd");
    label_cd->setGeometry(608, 80, 320, 320);

    slider_play = new QSlider(Qt::Horizontal, this);
    slider_play->setObjectName("slider_music");
    slider_play->setGeometry(588, 420, 360, 20);

    label_curr_time = new QLabel(this);
    label_curr_time->setGeometry(588, 440, 40, 30);
    label_curr_time->setText("00:00");

    label_total_time = new QLabel(this);
    label_total_time->setGeometry(908, 440, 40, 30);
    label_total_time->setText("--:--");
    label_total_time->setAlignment(Qt::AlignRight);

    QFont font;
    font.setPixelSize(10);
    label_curr_time->setFont(font);
    label_total_time->setFont(font);

    btn_like = new QPushButton(this);
    btn_like->setCheckable(true);
    btn_like->setObjectName("btn_like");
    btn_like->setGeometry(588, 505, 60, 60);

    btn_mode = new QPushButton(this);
    btn_mode->setObjectName("btn_mode");
    btn_mode->setGeometry(688, 505, 60, 60);
    btn_mode->setProperty("data-state", 0);    // 设置自定义属性，用于进行三种状态的切换

    btn_list = new QPushButton(this);
    btn_list->setObjectName("btn_list");
    btn_list->setGeometry(788, 505, 60, 60);

    btn_volume = new QPushButton(this);
    btn_volume->setObjectName("btn_volume");
    btn_volume->setGeometry(888, 505, 60, 60);

    slider_volume = new QSlider(Qt::Vertical, this);
    slider_volume->setObjectName("slider_volume");
    slider_volume->setRange(0, 100);
    slider_volume->setValue(50);
    slider_volume->setGeometry(960, 335, 20, 200);
    slider_volume->hide();    // 默认隐藏

    mode_loop = MODE_LIST;  // 默认列表循环

    connect(btn_back, &QPushButton::clicked, this, &MusicWidget::btnBackClicked);
    connect(btn_play, &QPushButton::clicked, this, &MusicWidget::btnPlayClicked);
    connect(btn_prev, &QPushButton::clicked, this, &MusicWidget::btnPrevClicked);
    connect(btn_next, &QPushButton::clicked, this, &MusicWidget::btnNextClicked);
    connect(btn_like, &QPushButton::clicked, this, &MusicWidget::btnLikeClicked);
    connect(btn_mode, &QPushButton::clicked, this, &MusicWidget::btnModeClicked);
    connect(btn_list, &QPushButton::clicked, this, &MusicWidget::btnListClicked);
    connect(btn_volume, &QPushButton::clicked, this, &MusicWidget::btnVolumeClicked);

    connect(list_musics, &QListWidget::itemClicked, this, &MusicWidget::listWidgetCliked);

    connect(slider_play, &QSlider::sliderReleased, this, &MusicWidget::sliderPlayReleased);

    connect(slider_volume, &QSlider::sliderReleased, this, &MusicWidget::sliderVolumeReleased);

    scanMusic();
}

void MusicWidget::btnBackClicked()
{
    /* 关闭线程 */
    stop_play_thread();

    parentWidget()->show();
    this->close();
}

void MusicWidget::scanMusic()
{
    QDir dir(MUSIC_PATH);

    QStringList filters;    /* 定义过滤器 */
    filters << "*.mp3" << "*.wav" << "*.flag";

    /* 获取目录下所有符合条件的文件 */
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files, QDir::Name);  // 按名字排序
    for (int i = 0; i < files.count(); i++) {
        music_files.append(files.at(i).fileName());
        QString list_item = QString::fromUtf8(files.at(i).fileName().replace(".mp3", "").toUtf8().data());
        list_musics->addItem(list_item.split("-").at(0) + "\n" + "----" + list_item.split("-").at(1));  // 添加到 listWidget
    }

    music_cnt = music_files.size();
    curr_indx = 0;
}

void MusicWidget::create_play_thread()
{
    if (!music_play) {
        thread = new QThread(this);
        music_play = new MusicPlay(music_files[curr_indx]);
        music_play->moveToThread(thread);

        connect(thread, &QThread::started, music_play, &MusicPlay::start);
        connect(music_play, &MusicPlay::play_complete, this, &MusicWidget::playComplete);
        connect(music_play, &MusicPlay::send_total_time, this, &MusicWidget::setTotalTime);
        connect(music_play, &MusicPlay::send_curr_time, this, &MusicWidget::setCurrTime);

        // 刷新列表 list_musics
        list_musics->setCurrentRow(curr_indx);
    }
}

void MusicWidget::stop_play_thread()
{
    if (music_play) {
        music_play->stop();

        thread->quit();
        thread->wait();

        delete music_play;
        music_play = nullptr;

        delete thread;
        thread = nullptr;
    }
}

void MusicWidget::btnPlayClicked()
{
    bool isChecked = btn_play->isChecked();  // 获取当前状态

    if (isChecked) {
        if (!music_play) {
            create_play_thread();
        }
        if (!thread->isRunning()) {  // 线程未启动
            thread->start();
        }
        else {
            music_play->resume();
        }
    }
    else {
        if (music_play) {
            music_play->pause();
        }
    }
}

void MusicWidget::btnPrevClicked()
{
    stop_play_thread();     // 停止当前播放

    curr_indx--;
    if (curr_indx < 0) {
        curr_indx = music_cnt - 1;
    }

    create_play_thread();
    if (btn_play->isChecked()) {
        thread->start();
    }
}

void MusicWidget::btnNextClicked()
{
    stop_play_thread();     // 停止当前播放

    curr_indx++;
    if (curr_indx >= music_cnt) {
        curr_indx = 0;
    }

    create_play_thread();
    if (btn_play->isChecked()) {
        thread->start();
    }
}

void MusicWidget::setCurrTime(int curr_time)
{
    /* 设置标签 */
    int minute = curr_time / 60;
    int second = curr_time % 60;

    QString music_position;
    music_position.clear();

    if (minute >= 10) {
        music_position = QString::number(minute, 10);
    } else {
        music_position = "0" + QString::number(minute, 10);
    }

    if (second >= 10) {
        music_position = music_position + ":" + QString::number(second, 10);
    } else {
        music_position = music_position + ":0" + QString::number(second, 10);
    }

    label_curr_time->setText(music_position);

    /* 刷新进度条 */
    if (!slider_play->isSliderDown()) {
        slider_play->setValue(curr_time);
    }
}

void MusicWidget::setTotalTime(int total_time)
{
    /* 设置标签 */
    int minute = total_time / 60;
    int second = total_time % 60;

    QString music_duration;
    music_duration.clear();

    if (minute >= 10) {
        music_duration = QString::number(minute, 10);  /* 把整数转为字符串，以十进制形式 */
    } else {
        music_duration = "0" + QString::number(minute, 10);
    }

    if (second >= 10) {
        music_duration = music_duration + ":" + QString::number(second, 10);
    } else {
        music_duration = music_duration + ":0" + QString::number(second, 10);
    }

    label_total_time->setText(music_duration);

    /* 设置进度条范围 */
    slider_play->setRange(0, total_time);
}

void MusicWidget::playComplete()
{
    stop_play_thread();

    /* 根据循环模式选择下一首音乐 */
    switch (mode_loop) {
    case MODE_LIST:
        curr_indx++;
        if (curr_indx >= music_cnt) {
            curr_indx = 0;
        }
        break;
    case MODE_SINGLE:
        break;
    case MODE_RANDOM:
        curr_indx = QRandomGenerator::global()->bounded(0, music_cnt);
        break;
    default:
        break;
    }

    create_play_thread();
    if (btn_play->isChecked()) {
        thread->start();
    }
}

void MusicWidget::listWidgetCliked(QListWidgetItem *item)
{
    stop_play_thread();
    curr_indx = list_musics->row(item);
    create_play_thread();
    if (btn_play->isChecked()) {
        thread->start();
    }
}

void MusicWidget::sliderPlayReleased()
{
    int play_position = slider_play->value();

    music_play->set_seek(play_position);
}

void MusicWidget::btnLikeClicked()
{
}

void MusicWidget::btnModeClicked()
{
    int state = btn_mode->property("data-state").toInt();
    mode_loop = (state + 1) % 3;
    btn_mode->setProperty("data-state", mode_loop);
    btn_mode->style()->polish(btn_mode); // 强制刷新样式
}

void MusicWidget::btnListClicked()
{
}

void MusicWidget::btnVolumeClicked()
{
    if (slider_volume->isVisible()) {
        slider_volume->setVisible(false);
    } else {
        slider_volume->setVisible(true);
    }
}

void MusicWidget::sliderVolumeReleased()
{
    if (music_play) {
        music_play->set_volume(slider_volume->value());
        int volume = slider_volume->value();
        qDebug() << "set volume: " << volume << endl;
    }
}
