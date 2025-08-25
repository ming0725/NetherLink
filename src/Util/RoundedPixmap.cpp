/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QPainter>
#include <QPainterPath>

#include "Util/RoundedPixmap.hpp"

/* namespace -------------------------------------------------------------- 80 // ! ----------------------------- 120 */
namespace Util {
/* function --------------------------------------------------------------- 80 // ! ----------------------------- 120 */
    RoundedPixmap::RoundedPixmap() {}

    RoundedPixmap::~RoundedPixmap() {}

    QPixmap RoundedPixmap::函数_圆角头像(const QPixmap &形参_图片, int 形参_尺寸) {
        QPixmap 画布(QSize(形参_尺寸, 形参_尺寸));

        画布.fill(Qt::transparent);

        QPainter p(&画布);

        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::SmoothPixmapTransform);

        QPainterPath path;

        path.addRoundedRect(QRectF(QPointF(0, 0), QSizeF(形参_尺寸, 形参_尺寸)), 形参_尺寸 / 2, 形参_尺寸 / 2);
        p.setClipPath(path);
        p.drawPixmap(0, 0, 形参_图片.scaled(形参_尺寸, 形参_尺寸, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));

        return (画布);
    }

    QPixmap RoundedPixmap::函数_圆角头像(const QPixmap &形参_图片, int 形参_尺寸, int 形参_半径) {
        QPixmap 画布(QSize(形参_尺寸, 形参_尺寸));

        画布.fill(Qt::transparent);

        QPainter p(&画布);

        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::SmoothPixmapTransform);

        QPainterPath path;

        path.addRoundedRect(QRectF(QPointF(0, 0), QSizeF(形参_尺寸, 形参_尺寸)), 形参_半径, 形参_半径);
        p.setClipPath(path);
        p.drawPixmap(0, 0, 形参_图片.scaled(形参_尺寸, 形参_尺寸, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));

        return (画布);
    }
}
