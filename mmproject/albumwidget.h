#ifndef ALBUMWIDGET_H
#define ALBUMWIDGET_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QStringList>
#include "commonresource.h"


class Albumwidget : public QWidget
{
    Q_OBJECT
public:
    explicit Albumwidget(QWidget *parent = nullptr);

private:
    QPushButton *btn_back;
    QPushButton *btn_prev;
    QPushButton *btn_next;
    QLabel *show_photo;
    QLabel *photo_info;
    QLabel *photo_num;

    QStringList photo_files;  /* 字符串数组: 存储照片文件名 */
    int photo_cnt;      /* 照片总数 */
    int curr_indx;      /* 当前索引 */

    void scanImages();      /* 照片扫描函数 */
    void showPhoto();       /* 照片展示函数 */

private slots:
    void btnBackClicked();
    void btnPrevClicked();
    void btnNextClicked();

};

#endif // ALBUMWIDGET_H
