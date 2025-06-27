#include <QApplication>
#include <QPixmapCache>
#include <QFontDatabase>
#include "Login.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QPixmapCache::setCacheLimit(20480);

//    // 从资源加载字体
//    int fontId = QFontDatabase::addApplicationFont(":/resources/font/MinecraftAE.ttf");
//    if (fontId != -1) {
//        QString family = QFontDatabase::applicationFontFamilies(fontId).at(0);
//        QFont font(family);
//
//        // 像素字体优化设置
//        font.setHintingPreference(QFont::PreferFullHinting);  // 使用完整的hinting
//        font.setStyleStrategy(static_cast<QFont::StyleStrategy>(
//            QFont::NoAntialias |     // 禁用抗锯齿
//            QFont::PreferBitmap |    // 优先使用位图
//            QFont::NoSubpixelAntialias  // 禁用子像素抗锯齿
//        ));
//
//        // 设置全局字体
//        QApplication::setFont(font);
//
//        // 设置应用程序范围的字体渲染提示
//        QApplication::setAttribute(Qt::AA_Use96Dpi, true);  // 使用96 DPI渲染
//    }

    Login l;
    l.show();
    return a.exec();
}
