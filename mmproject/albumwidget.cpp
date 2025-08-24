#include "albumwidget.h"
#include <QDir>
#include <QDebug>


Albumwidget::Albumwidget(QWidget *parent) : QWidget(parent)
{
    this->resize(1024, 600);

    btn_back = new QPushButton(this);
    btn_back->setText("返回");
    btn_back->setGeometry(20, 20, 100, 40);

    btn_prev = new QPushButton(this);
    btn_prev->setObjectName("btn_prev_photo");
    btn_prev->setGeometry(66, 270, 60, 60);

    btn_next = new QPushButton(this);
    btn_next->setObjectName("btn_next_photo");
    btn_next->setGeometry(898, 270, 60, 60);

    show_photo = new QLabel(this);
    show_photo->setObjectName("v4l2_photo");
    show_photo->setGeometry(192, 100, 640, 400);

    photo_num = new QLabel(this);
    photo_num->setGeometry(472, 40, 80, 40);

    photo_info = new QLabel(this);
    photo_info->setGeometry(192, 530, 640, 40);

    scanImages();
    showPhoto();

    connect(btn_back, &QPushButton::clicked, this, &Albumwidget::btnBackClicked);
    connect(btn_prev, &QPushButton::clicked, this, &Albumwidget::btnPrevClicked);
    connect(btn_next, &QPushButton::clicked, this, &Albumwidget::btnNextClicked);
}

void Albumwidget::scanImages()
{
    QDir dir(IMAGE_SAVE_PATH);

    QStringList filters;    /* 定义过滤器 */
    filters << "*.jpg" << "*.jpeg" << "*.png" << "*.bmp" << "*.gif";

    /* 获取目录下所有符合条件的文件 */
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files, QDir::Time);  // 按时间排序
    /* 把文件名加入数组 */
    for (int i = 0; i < files.count(); i++) {
        photo_files.append(files.at(i).fileName());
    }

    photo_cnt = photo_files.size();
    curr_indx = 0;
}

void Albumwidget::showPhoto()
{
    QString photo_path = QString(IMAGE_SAVE_PATH) + "/" + photo_files[curr_indx];

    QPixmap pixmap(photo_path);
    if (!pixmap.isNull()) {
        show_photo->setPixmap(pixmap);
        show_photo->setAlignment(Qt::AlignCenter);
        photo_num->setText(QString::number(curr_indx + 1) + "/" + QString::number(photo_cnt));
        photo_info->setText("当前照片: " + photo_path);
    } else {
        photo_num->setText("-----");
        photo_info->setText("无法加载照片: " + photo_path);
        qWarning() << "加载照片失败: " << photo_path << endl;
    }
}

void Albumwidget::btnBackClicked()
{
    parentWidget()->show();
    this->close();
}

void Albumwidget::btnPrevClicked()
{
    curr_indx--;
    if (curr_indx < 0) {
        curr_indx = photo_cnt - 1;
    }

    showPhoto();
}

void Albumwidget::btnNextClicked()
{
    curr_indx++;
    if (curr_indx >= photo_cnt) {
        curr_indx = 0;
    }

    showPhoto();
}
