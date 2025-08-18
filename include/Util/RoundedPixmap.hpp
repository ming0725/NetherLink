#ifndef INCLUDE_UTIL_ROUNDEDPIXMAP_H

#define INCLUDE_UTIL_ROUNDEDPIXMAP_H
#include <QPixmap>

namespace Util {

    class RoundedPixmap {
        public:
            RoundedPixmap();

            ~RoundedPixmap();

            static QPixmap 函数_圆角头像(const QPixmap &形参_图片, int 形参_尺寸);

            static QPixmap 函数_圆角头像(const QPixmap &形参_图片, int 形参_尺寸, int 形参_半径);
    };
}

#endif // INCLUDE/UTIL/ROUNDEDPIXMAP_H
