/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_UTIL_ROUNDED_PIXMAP

#define INCLUDE_UTIL_ROUNDED_PIXMAP

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QPixmap>

/* namespace -------------------------------------------------------------- 80 // ! ----------------------------- 120 */
namespace Util {

/**//* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
    class RoundedPixmap {
        public:
            RoundedPixmap();

            ~RoundedPixmap();

            static QPixmap 函数_圆角头像(const QPixmap &形参_图片, int 形参_尺寸);

            static QPixmap 函数_圆角头像(const QPixmap &形参_图片, int 形参_尺寸, int 形参_半径);
    };
}

#endif /* INCLUDE_UTIL_ROUNDED_PIXMAP */
