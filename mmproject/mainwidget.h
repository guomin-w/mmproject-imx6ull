#ifndef MAINWIDGET_H
#define MAINWIDGET_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>

class MainWidget : public QWidget
{
    Q_OBJECT

public:
    MainWidget(QWidget *parent = nullptr);
    ~MainWidget();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QPushButton *btn_camera;
    QPushButton *btn_album;
    QPushButton *btn_music;
    QPushButton *btn_video;
    QLabel *label_camera;
    QLabel *label_album;
    QLabel *label_music;
    QLabel *label_video;

private slots:
    void btnCameraClicked();
    void btnAlbumClicked();
    void btnMusicClicked();
    void btnVideoClicked();
};
#endif // MAINWIDGET_H
