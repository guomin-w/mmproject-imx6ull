#include "camerawidget.h"
#include <QDebug>
#include <QDir>
#include <QDateTime>
#include <QTimer>


CameraWidget::CameraWidget(QWidget *parent) : QWidget(parent)
{
    this->resize(1024, 600);

    btn_back = new QPushButton(this);
    btn_back->setText("返回");
    btn_back->setGeometry(20, 20, 80, 40);

    btn_switch = new QPushButton(this);
    btn_switch->setCheckable(true);
    btn_switch->setGeometry(472, 30, 60, 30);
    btn_switch->setObjectName("btn_switch");

    label_switch = new QLabel(this);
    label_switch->setGeometry(540, 30, 200, 30);
    label_switch->setText("摄像头状态: 关闭");

    v4l2_photo = new QLabel(this);
    v4l2_photo->setGeometry(192, 100, 640, 400);
    v4l2_photo->setObjectName("v4l2_photo");

    btn_photo = new QPushButton(this);
    btn_photo->setText("拍照");
    btn_photo->setGeometry(472, 530, 80, 40);

    label_photo = new QLabel(this);
    label_photo->setGeometry(560, 500, 450, 100);
    label_photo->setWordWrap(true);  // 启用文本换行
    label_photo->setAlignment(Qt::AlignVCenter);

    connect(btn_back, &QPushButton::clicked, this, &CameraWidget::btnBackClicked);
    connect(btn_switch, &QPushButton::clicked, this, &CameraWidget::btnSwitchClicked);
    connect(btn_photo, &QPushButton::clicked, this, &CameraWidget::btnPhotoClicked);
}

void CameraWidget::btnBackClicked()
{
    if (btn_switch->isChecked()) {
        btn_switch->click();
    }
    parentWidget()->show();
    this->close();
}

void CameraWidget::btnSwitchClicked()
{
    bool isChecked = btn_switch->isChecked();  // 获取当前状态

    /* 1. checked 状态 */
    if (isChecked) {
        m_thread = new QThread(this);
        m_capture = new CameraCapture();
        m_capture->moveToThread(m_thread);

        connect(m_thread, &QThread::started, m_capture, &CameraCapture::start);
        connect(m_capture, &CameraCapture::frameReady, this, &CameraWidget::displayFrame);

        m_thread->start();

        label_switch->setText("摄像头状态: 打开");
    }
    /* 2. unchecked 状态 */
    else {
        m_capture->stop();
        m_thread->quit();
        m_thread->wait();

        delete m_capture;
        delete m_thread;

        label_switch->setText("摄像头状态: 关闭");

        // 3秒后自动清除图片
        QTimer::singleShot(3000, [this]() {
            v4l2_photo->setPixmap(QPixmap());
        });
    }
}

void CameraWidget::displayFrame(const QByteArray &frame)
{
    if (frame.isEmpty()) {
        qWarning() << "Received empty frame data";
        return;
    }

    QImage img;
    if (!img.loadFromData(frame, "JPEG")) {
        qWarning() << "Failed to decode MJPEG frame" << endl;
        return;
    }

    QPixmap pixmap;
    pixmap = QPixmap::fromImage(img);

    v4l2_photo->setPixmap(pixmap);
}

void CameraWidget::btnPhotoClicked()
{
    /* 照片路径 */
    QString photoPath = QString("%1/capture_%2.jpg")
            .arg(IMAGE_SAVE_PATH)
            .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmsszzz"));

    const QPixmap *pixmapPtr = v4l2_photo->pixmap();
    if (pixmapPtr && !pixmapPtr->isNull()) {
        QPixmap pixmap = *pixmapPtr;
        bool success = pixmap.save(photoPath);
        if (success) {
            label_photo->setText("照片保存成功:\n" + photoPath);
        } else {
            label_photo->setText("照片保存失败!");
        }
    } else {
        label_photo->setText("未检测到图像!");
    }
    // 5秒后自动清除文本
    QTimer::singleShot(5000, [this]() {
        label_photo->clear();
    });
}
