#ifndef MUSICWIDGET_H
#define MUSICWIDGET_H

#include <QWidget>
#include <QPushButton>
#include <QListWidget>
#include <QLabel>
#include <QSlider>
#include <QStringList>
#include <QThread>
#include "musicplay.h"


#define MODE_LIST 0
#define MODE_SINGLE 1
#define MODE_RANDOM 2


class MusicWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MusicWidget(QWidget *parent = nullptr);

private:
    QPushButton *btn_back;
    QPushButton *btn_prev;
    QPushButton *btn_next;
    QPushButton *btn_play;
    QPushButton *btn_like;
    QPushButton *btn_mode;
    QPushButton *btn_list;
    QPushButton *btn_volume;
    QListWidget *list_musics;
    QLabel *label_cd;
    QSlider *slider_play;     /* 播放进度条 */
    QSlider *slider_volume;   /* 音量滑动条 */
    QLabel *label_total_time;
    QLabel *label_curr_time;

    QStringList music_files;  /* 字符串数组: 存储音乐文件名 */
    int music_cnt;       /* 音乐总数 */
    int curr_indx;       /* 当前索引 */

    int mode_loop;   /* 循环模式: 列表循环0、单曲循环1、随机播放2 */

    QThread *thread = nullptr;
    MusicPlay *music_play = nullptr;

    void scanMusic();    /* 扫描音乐 */
    void create_play_thread();    /* 创建音乐播放线程 */
    void stop_play_thread();      /* 停止音乐播放线程 */

private slots:
    void btnBackClicked();
    void btnPlayClicked();
    void btnPrevClicked();
    void btnNextClicked();
    void btnLikeClicked();
    void btnModeClicked();
    void btnListClicked();
    void btnVolumeClicked();
    void listWidgetCliked(QListWidgetItem*);    /* 列表单击 */
    void sliderPlayReleased();                  /* 播放进度条松开 */
    void sliderVolumeReleased();                /* 音量进度条松开 */

    void setCurrTime(int curr_time);      /* 获取当前播放时间 */
    void setTotalTime(int total_time);    /* 获取总时长 */
    void playComplete();    /* 当前歌曲播放完成 */
};

#endif // MUSICWIDGET_H
