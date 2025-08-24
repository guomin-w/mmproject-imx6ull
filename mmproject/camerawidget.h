#ifndef CAMERAWIDGET_H
#define CAMERAWIDGET_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QThread>
#include "cameracapture.h"
#include "commonresource.h"


class CameraWidget : public QWidget
{
    Q_OBJECT
public:
    explicit CameraWidget(QWidget *parent = nullptr);

private:
    QPushButton *btn_back;
    QPushButton *btn_switch;
    QPushButton *btn_photo;
    QLabel *label_switch;
    QLabel *v4l2_photo;
    QLabel *label_photo;

    QThread *m_thread;
    CameraCapture *m_capture;

private slots:
    void btnBackClicked();
    void btnSwitchClicked();
    void btnPhotoClicked();

    void displayFrame(const QByteArray &frame);
};

#endif // CAMERAWIDGET_H
