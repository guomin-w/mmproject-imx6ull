#include "mainwidget.h"

#include <QApplication>
#include <QFile>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    /* 指定样式文件 */
    QFile file(":/style.qss");
    if (file.exists()) {
        file.open(QFile::ReadOnly);
        QString styleSheet = QLatin1String(file.readAll());    /* 以字符串的方式保存读出的结果 */
        qApp->setStyleSheet(styleSheet);    /* 设置全局样式 */
        file.close();
    }

    MainWidget w;
    w.show();
    return a.exec();
}
