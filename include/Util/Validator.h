/**
 * @file Validator.h
 * @version 1.0.0
 * @author 落羽行歌 (2481036245@qq.com)
 * @date 2025-08-14 4 09:16:04
 * @brief 【描述】
 */

/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_UTIL_VALIDATOR

#define INCLUDE_UTIL_VALIDATOR

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QRegularExpression>
#include <QString>

/* namespace -------------------------------------------------------------- 80 // ! ----------------------------- 120 */
namespace Util {

/**//* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
    class Validator {
        public:
            static bool 函数_是否为有效邮箱(const QString& 邮箱);

        private:
            static const QRegularExpression 邮箱正则表达式;
    };
}

#endif /* INCLUDE_UTIL_VALIDATOR */
