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

#include <QLineEdit>

#include <QRegularExpression>

/* namespace -------------------------------------------------------------- 80 // ! ----------------------------- 120 */
namespace Util {

/**//* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
    class Validator {
        public:
            Validator();

            ~Validator();

            static bool 函数_是否为有效邮箱(const QString& 形参_邮箱);

            static bool 函数_检查输入框信息合法性(QWidget* 形参_目标窗口, QLineEdit* 形参_邮箱输入框);

            static bool 函数_检查输入框信息合法性(QWidget* 形参_目标窗口, QLineEdit* 形参_邮箱输入框, QLineEdit* 形参_密码输入框);

            static bool 函数_检查输入框信息合法性(QWidget* 形参_目标窗口, QString avatarUrl, QLineEdit* 形参_昵称输入框, QLineEdit* 形参_邮箱输入框, QLineEdit* 形参_验证码输入框, QLineEdit* 形参_密码输入框, QLineEdit* 形参_确认密码输入框);

        private:
            static const QRegularExpression 成员变量_邮箱正则表达式;
    };
}

#endif /* INCLUDE_UTIL_VALIDATOR */
