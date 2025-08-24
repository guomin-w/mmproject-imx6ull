#include "mainwidget.h"
#include "camerawidget.h"
#include "albumwidget.h"
#include "musicwidget.h"
#include "videowidget.h"
#include <QPainter>

MainWidget::MainWidget(QWidget *parent)
    : QWidget(parent)
{
    this->resize(1024, 600);

    btn_camera = new QPushButton(this);
    btn_camera->setGeometry(100, 100, 80, 80);
    btn_camera->setObjectName("btn_camera");

    btn_album = new QPushButton(this);
    btn_album->setGeometry(280, 100, 80, 80);
    btn_album->setObjectName("btn_album");

    btn_music = new QPushButton(this);
    btn_music->setGeometry(100, 300, 80, 80);
    btn_music->setObjectName("btn_music");

    btn_video = new QPushButton(this);
    btn_video->setGeometry(280, 300, 80, 80);
    btn_video->setObjectName("btn_video");

    label_camera = new QLabel(this);
    label_camera->setText("相机");
    label_camera->setAlignment(Qt::AlignCenter);    // 文字居中
    label_camera->setGeometry(100, 180, 80, 40);

    label_album = new QLabel(this);
    label_album->setText("相册");
    label_album->setAlignment(Qt::AlignCenter);    // 文字居中
    label_album->setGeometry(280, 180, 80, 40);

    label_music = new QLabel(this);
    label_music->setText("音乐");
    label_music->setAlignment(Qt::AlignCenter);    // 文字居中
    label_music->setGeometry(100, 380, 80, 40);

    label_video = new QLabel(this);
    label_video->setText("视频");
    label_video->setAlignment(Qt::AlignCenter);    // 文字居中
    label_video->setGeometry(280, 380, 80, 40);

    connect(btn_camera, &QPushButton::clicked, this, &MainWidget::btnCameraClicked);
    connect(btn_album, &QPushButton::clicked, this, &MainWidget::btnAlbumClicked);
    connect(btn_music, &QPushButton::clicked, this, &MainWidget::btnMusicClicked);
    connect(btn_video, &QPushButton::clicked, this, &MainWidget::btnVideoClicked);
}

MainWidget::~MainWidget()
{
}

void MainWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    QPixmap bg(":/images/background.png");

    if(!bg.isNull()) {
        // 缩放图片适应窗口
        painter.drawPixmap(rect(), bg);
    } else {
        // 后备方案：绘制纯色背景
        painter.fillRect(rect(), Qt::lightGray);
    }
}

void MainWidget::btnCameraClicked()
{
    CameraWidget *camera_widget = new CameraWidget(this);
    camera_widget->setWindowFlags(Qt::Window);  // 设置为独立窗口
    camera_widget->show();
    this->hide();
}

void MainWidget::btnAlbumClicked()
{
    Albumwidget *album_widget = new Albumwidget(this);
    album_widget->setWindowFlags(Qt::Window);  // 设置为独立窗口
    album_widget->show();
    this->hide();
}

void MainWidget::btnMusicClicked()
{
    MusicWidget *music_widget = new MusicWidget(this);
    music_widget->setWindowFlags(Qt::Window);  // 设置为独立窗口
    music_widget->show();
    this->hide();
}

void MainWidget::btnVideoClicked()
{
    VideoWidget *video_widget = new VideoWidget(this);
    video_widget->setWindowFlags(Qt::Window);  // 设置为独立窗口
    video_widget->show();
    this->hide();
}
