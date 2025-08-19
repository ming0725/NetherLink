#pragma once

#include <QPushButton>
#include <QColor>
#include <QPropertyAnimation>

class CustomPushButton : public QPushButton
{
Q_OBJECT
    Q_PROPERTY(int radius          READ radius          WRITE setRadius)
    Q_PROPERTY(QColor normalColor  READ normalColor     WRITE setNormalColor)
    Q_PROPERTY(QColor hoverColor   READ hoverColor      WRITE setHoverColor)
    Q_PROPERTY(QColor pressColor   READ pressColor      WRITE setPressColor)
    Q_PROPERTY(QColor disabledColor READ disabledColor  WRITE setDisabledColor)
    Q_PROPERTY(QColor currentColor READ currentColor    WRITE setCurrentColor USER true)
    Q_PROPERTY(QColor textColor    READ textColor       WRITE setTextColor)
    Q_PROPERTY(QColor borderColor  READ borderColor     WRITE setBorderColor)
    Q_PROPERTY(int borderWidth     READ borderWidth     WRITE setBorderWidth)
    Q_PROPERTY(bool isFlat         READ isFlat          WRITE setFlat)

public:
    explicit CustomPushButton(QWidget* parent = nullptr);
    explicit CustomPushButton(const QString& text, QWidget* parent = nullptr);
    ~CustomPushButton() override;

    // 圆角
    int  radius() const;
    void setRadius(int radius);

    // 各种色
    QColor normalColor() const;
    void   setNormalColor(const QColor& c);

    QColor hoverColor() const;
    void   setHoverColor(const QColor& c);

    QColor pressColor() const;
    void   setPressColor(const QColor& c);

    QColor disabledColor() const;
    void   setDisabledColor(const QColor& c);

    QColor currentColor() const;
    void   setCurrentColor(const QColor& c);

    QColor textColor() const;
    void   setTextColor(const QColor& c);

    QColor borderColor() const;
    void   setBorderColor(const QColor& c);

    int borderWidth() const;
    void setBorderWidth(int w);

    bool isFlat() const;
    void setFlat(bool flat);

    // 动画时长
    void setAnimationDuration(int ms);
    int  animationDuration() const;

    // 预设样式
    void setDefaultStyle();
    void setPrimaryStyle();
    void setSuccessStyle();
    void setWarningStyle();
    void setDangerStyle();

protected:
    void enterEvent(QEnterEvent*  event) override;
    void leaveEvent(QEvent*       event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent*   event) override;
    bool event(QEvent* e) override;

private:
    void initializeButton();
    void applyStateColor();

private:
    int                m_radius;
    QColor             m_normalColor;
    QColor             m_hoverColor;
    QColor             m_pressColor;
    QColor             m_disabledColor;
    QColor             m_currentColor;
    QColor             m_textColor;
    QColor             m_borderColor;
    int                m_borderWidth;
    bool               m_isFlat;
    int                m_animationDuration;
    bool               m_isHovered;
    bool               m_isPressed;
    QPropertyAnimation* m_colorAnimation;
};
