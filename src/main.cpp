/**
 * @file main.cpp
 * @version 1.0.0
 * @author 落羽行歌 (2481036245@qq.com)
 * @date 2025-08-11 周一 10:27:41
 * @brief 【描述】 主函数
 */

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QApplication>
#include <QFontDatabase>
#include <QPixmapCache>

#include "View/Mainwindow/Login.h"

/* function --------------------------------------------------------------- 80 // ! ----------------------------- 120 */

int main(int argc, char* argv[]) {
    QApplication a(argc, argv);

    QApplication::setAttribute(Qt::AA_Use96Dpi, true);
    QPixmapCache::setCacheLimit(20480);

    // QPalette palette;
    // palette.setColor(QPalette::WindowText, Qt::black);
    // palette.setColor(QPalette::Text, Qt::black);
    // palette.setColor(QPalette::ButtonText, Qt::black);
    // palette.setColor(QPalette::PlaceholderText, QColor(0x808080));
    // palette.setColor(QPalette::ToolTipText, Qt::black);
    // QApplication::setPalette(palette);
    Login 界面_登录;

    界面_登录.show();

    return (a.exec());
}
