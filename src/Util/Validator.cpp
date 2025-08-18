/**
 * @file Validator.cpp
 * @version 1.0.0
 * @author 落羽行歌 (2481036245@qq.com)
 * @date 2025-08-14 周四 09:32:03
 * @brief 【描述】
 */

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

// #include "Util/ToastTip.hpp"
#include "Util/Validator.hpp"
#include "View/Mainwindow/NotificationManager.h"

/* namespace -------------------------------------------------------------- 80 // ! ----------------------------- 120 */
namespace Util {
/* variable --------------------------------------------------------------- 80 // ! ----------------------------- 120 */
    const QRegularExpression Validator::成员变量_邮箱正则表达式("^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\\.[A-Za-z]{2,}$");

/* function --------------------------------------------------------------- 80 // ! ----------------------------- 120 */

    Validator::Validator() {}

    Validator::~Validator() {}

    /**
     * @brief 检查邮箱合法性
     *
     * @param 形参_邮箱 用户邮箱
     *
     * @return true 邮箱合法
     * @return false 邮箱非法
     */
    bool Validator::函数_是否为有效邮箱(const QString& 形参_邮箱) {
        return (成员变量_邮箱正则表达式.match(形参_邮箱).hasMatch());
    }

    // TODO 设置输入框焦点
    bool Validator::函数_检查输入框信息合法性(QWidget* 形参_目标窗口, QLineEdit* 形参_邮箱输入框) {
        QString 形参_邮箱 = 形参_邮箱输入框->text().trimmed();

        if (形参_邮箱.isEmpty()) {
            NotificationManager::instance().showMessage(形参_目标窗口, NotificationManager::Error, "请输入邮箱");

            return (false);
        } else if (!函数_是否为有效邮箱(形参_邮箱)) {
            NotificationManager::instance().showMessage(形参_目标窗口, NotificationManager::Error, "邮箱格式不正确");

            return (false);
        } else {
            return (true);
        }
    }

    // TODO 设置输入框焦点
    bool Validator::函数_检查输入框信息合法性(QWidget* 形参_目标窗口, QLineEdit* 形参_邮箱输入框, QLineEdit* 形参_密码输入框) {
        QString 形参_邮箱 = 形参_邮箱输入框->text().trimmed();
        QString 形参_密码 = 形参_密码输入框->text();

        if (形参_邮箱.isEmpty()) {
            NotificationManager::instance().showMessage(形参_目标窗口, NotificationManager::Error, "请输入邮箱");
            形参_邮箱输入框->setFocus();

            return (false);
        } else if (!函数_是否为有效邮箱(形参_邮箱)) {
            NotificationManager::instance().showMessage(形参_目标窗口, NotificationManager::Error, "邮箱格式不正确");
            形参_邮箱输入框->selectAll();
            形参_邮箱输入框->setFocus();

            return (false);
        } else if (形参_密码.isEmpty()) {
            NotificationManager::instance().showMessage(形参_目标窗口, NotificationManager::Error, "请输入密码");
            形参_密码输入框->setFocus();

            return (false);
        } else {
            return (true);
        }
    }

    // TODO 设置输入框焦点
    bool Validator::函数_检查输入框信息合法性(QWidget* 形参_目标窗口, QString avatarUrl, QLineEdit* 形参_昵称输入框, QLineEdit* 形参_邮箱输入框, QLineEdit* 形参_验证码输入框, QLineEdit* 形参_密码输入框, QLineEdit* 形参_确认密码输入框) {
        QString 形参_昵称 = 形参_昵称输入框->text().trimmed();
        QString 形参_邮箱 = 形参_邮箱输入框->text().trimmed();
        QString 形参_验证码 = 形参_验证码输入框->text().trimmed();
        QString 形参_密码 = 形参_密码输入框->text();
        QString 形参_确认密码 = 形参_确认密码输入框->text();

        if (形参_昵称.isEmpty()) {
            NotificationManager::instance().showMessage(形参_目标窗口, NotificationManager::Error, "请输入用户名");

            return (false);
        } else if (形参_邮箱.isEmpty()) {
            NotificationManager::instance().showMessage(形参_目标窗口, NotificationManager::Error, "请输入邮箱");

            return (false);
        } else if (!函数_是否为有效邮箱(形参_邮箱)) {
            NotificationManager::instance().showMessage(形参_目标窗口, NotificationManager::Error, "邮箱格式不正确");

            return (false);
        } else if (形参_验证码.isEmpty()) {
            NotificationManager::instance().showMessage(形参_目标窗口, NotificationManager::Error, "请输入验证码");

            return (false);
        } else if (形参_密码.isEmpty()) {
            NotificationManager::instance().showMessage(形参_目标窗口, NotificationManager::Error, "请输入密码");

            return (false);
        } else if (形参_确认密码.isEmpty()) {
            NotificationManager::instance().showMessage(形参_目标窗口, NotificationManager::Error, "请确认密码");

            return (false);
        } else if (形参_密码 != 形参_确认密码) {
            NotificationManager::instance().showMessage(形参_目标窗口, NotificationManager::Error, "两次输入的密码不一致");

            return (false);
        } else if (avatarUrl.isEmpty()) {
            NotificationManager::instance().showMessage(形参_目标窗口, NotificationManager::Error, "请上传头像");

            return (false);
        } else {
            return (true);
        }
    }
}
