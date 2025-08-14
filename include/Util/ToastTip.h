/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_UTIL_MESSAGE_BOX

#define INCLUDE_UTIL_MESSAGE_BOX

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QPropertyAnimation>
#include <QWidget>

/* namespace -------------------------------------------------------------- 80 // ! ----------------------------- 120 */
namespace Ui {
    class ToastTip;
}     // namespace Ui

namespace Util {

/**//* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
    class ToastTip : public QWidget {
        Q_OBJECT

        public:
            explicit ToastTip(QWidget* parent = nullptr);

            ~ToastTip();

        private:
            Ui::ToastTip* ui;
            enum Type {
                Error = 0,
                Success = 1,
            };

            QPropertyAnimation* animation;
            bool isShowing = false;

        private:
            // 单例接口
            static ToastTip& instance();

            // 成功消息
            void showMessage(const QString& message, Type type = Success);

            // 失败消息
            void showMessage(const QString& message, Type type, QWidget* targetWidget);

            void startAnimation(bool show); // 保留给全屏居中版本使用

        protected:
            void paintEvent(QPaintEvent* event) override;
    };
}

#endif /* INCLUDE_UTIL_MESSAGE_BOX */
