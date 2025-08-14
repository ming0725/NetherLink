/**
 * @file main.cpp
 * @version 1.0.0
 * @author 落羽行歌 (2481036245@qq.com)
 * @date 2025-08-11 周一 10:27:41
 * @brief 【描述】 主函数
 */

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QApplication>
#include <QPixmapCache>

#include "Window/Login.h"

/* function --------------------------------------------------------------- 80 // ! ----------------------------- 120 */

int main(int argc, char* argv[]) {
    QApplication a(argc, argv);

    QPixmapCache::setCacheLimit(20480);

    Window::Login 界面_登录;

    界面_登录.show();

    return (a.exec());
}
