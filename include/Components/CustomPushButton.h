/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_COMPONENTS_CUSTOM_PUSH_BUTTON

#define INCLUDE_COMPONENTS_CUSTOM_PUSH_BUTTON

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QColor>
#include <QPropertyAnimation>
#include <QPushButton>

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class CustomPushButton : public QPushButton {
    Q_OBJECT Q_PROPERTY(int radius READ radius WRITE setRadius) Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor USER true) Q_PROPERTY(QColor hoverColor READ hoverColor WRITE setHoverColor) Q_PROPERTY(QColor pressColor READ pressColor WRITE setPressColor) Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor) Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor) Q_PROPERTY(int borderWidth READ borderWidth WRITE setBorderWidth) Q_PROPERTY(bool isFlat READ isFlat WRITE setFlat)

    public:
        explicit CustomPushButton(QWidget* parent = nullptr);

        explicit CustomPushButton(const QString& text, QWidget* parent = nullptr);

        ~CustomPushButton() override;

        // 圆角相关
        int radius() const;

        void setRadius(int radius);

        // 颜色相关
        QColor backgroundColor() const;

        void setBackgroundColor(const QColor& color);

        QColor hoverColor() const;

        void setHoverColor(const QColor& color);

        QColor pressColor() const;

        void setPressColor(const QColor& color);

        QColor textColor() const;

        void setTextColor(const QColor& color);

        QColor borderColor() const;

        void setBorderColor(const QColor& color);

        // 边框相关
        int borderWidth() const;

        void setBorderWidth(int width);

        // 扁平化
        bool isFlat() const;

        void setFlat(bool flat);

        // 动画时长
        void setAnimationDuration(int duration);

        int animationDuration() const;

        // 预设样式
        void setDefaultStyle();

        void setPrimaryStyle();

        void setSuccessStyle();

        void setWarningStyle();

        void setDangerStyle();

        // 自定义样式
        void setCustomStyle(const QColor& background, const QColor& hover, const QColor& press, const QColor& text);

    protected:
        void enterEvent(QEnterEvent* event) override;

        void leaveEvent(QEvent* event) override;

        void mousePressEvent(QMouseEvent* event) override;

        void mouseReleaseEvent(QMouseEvent* event) override;

        void paintEvent(QPaintEvent* event) override;

        bool event(QEvent* e) override;

    private:
        void initializeButton();

        void updateStyle();

    private:
        int m_radius;
        QColor m_backgroundColor;
        QColor m_hoverColor;
        QColor m_pressColor;
        QColor m_textColor;
        QColor m_borderColor;
        int m_borderWidth;
        bool m_isFlat;
        int m_animationDuration;
        bool m_isHovered;
        bool m_isPressed;
        QPropertyAnimation* m_backgroundAnimation;
};

#endif /* INCLUDE_COMPONENTS_CUSTOM_PUSH_BUTTON */
