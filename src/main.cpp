/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QApplication>
#include <QFontDatabase>
#include <QPixmapCache>

#include "View/Mainwindow/Login.h"

/* function --------------------------------------------------------------- 80 // ! ----------------------------- 120 */

int main(int argc, char*argv[]) {
    QApplication a(argc, argv);

    QPixmapCache::setCacheLimit(20480);

    // 思源黑体
    QFont font("Source Han Sans Medium");

    font.setHintingPreference(QFont::PreferFullHinting);
    font.setStyleStrategy(static_cast<QFont::StyleStrategy>(
                                  QFont::PreferAntialias |
                                  QFont::PreferQuality |
                                  QFont::PreferMatch
                          ));

    QApplication::setFont(font);
    QApplication::setAttribute(Qt::AA_Use96Dpi, true);

    QPalette palette;
    palette.setColor(QPalette::WindowText, Qt::black);
    palette.setColor(QPalette::Text, Qt::black);
    palette.setColor(QPalette::ButtonText, Qt::black);
    palette.setColor(QPalette::PlaceholderText, QColor(0x808080));
    palette.setColor(QPalette::ToolTipText, Qt::black);

    QApplication::setPalette(palette);

    Login l;

    l.show();

    return (a.exec());
}
