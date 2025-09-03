/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_UTIL_TOAST_TIP

#define INCLUDE_UTIL_TOAST_TIP

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QLabel>
#include <QPropertyAnimation>

/* namespace -------------------------------------------------------------- 80 // ! ----------------------------- 120 */
namespace Ui {
    class ToastTip;
} // namespace Ui

namespace Util {

/**//* class --枚举_消息类型------------------------------------------------------- 80 // ! ----------------------------- 120 */
    class ToastTip : public QWidget {
        Q_OBJECT

        public:
            explicit ToastTip(QWidget* parent = nullptr);

            ~ToastTip();

            // 单例接口
            static ToastTip& 函数_实例();

            enum class 枚举_消息类型 {
                ENUM_ERROR,
                ENUM_SUCCESS,
            };

            // 居中显示消息
            void 函数_显示消息(枚举_消息类型 形参_消息类型, const QString& 形参_消息内容);

            // 顶部显示消息
            void 函数_显示消息(QWidget* 形参_目标窗口, 枚举_消息类型 形参_消息类型, const QString& 形参_消息内容);

            void startAnimation(bool show); // 保留给全屏居中版本使用

        protected:
            void paintEvent(QPaintEvent* event) override;

        private:
            Ui::ToastTip* ui;
            QLabel* 界面_图片标签;
            QLabel* 界面_标题标签;
            QLabel* 界面_内容标签;

        private:
            QString 成员变量_图片;
            QString 成员变量_标题;
            QString 成员变量_内容;
            QPropertyAnimation* animation;
            bool 成员变量_正在显示 = false;
    };
}

#endif /* INCLUDE_UTIL_TOAST_TIP */
