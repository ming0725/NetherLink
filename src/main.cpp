/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QApplication>
#include <QFontDatabase>
#include <QPixmapCache>

#include "View/Mainwindow/Login.h"

/* function --------------------------------------------------------------- 80 // ! ----------------------------- 120 */

int main(int argc, char*argv[]) {
    QApplication a(argc, argv);
    QApplication::setAttribute(Qt::AA_Use96Dpi, true);
    QPixmapCache::setCacheLimit(20480);

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
