#pragma once

#include <QImage>
#include <QString>
#include <QWidget>

#include <functional>

class QEvent;
class QMouseEvent;
class QPaintEvent;
class QVariantAnimation;

class ProfileAvatarPreview final : public QWidget
{
public:
    explicit ProfileAvatarPreview(QWidget* parent = nullptr);

    void setAvatarSource(const QString& source);

    std::function<void(const QImage&)> avatarImageSelected;

protected:
    bool event(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void animateHover(qreal target);

    QString m_avatarSource;
    qreal m_hoverProgress = 0.0;
    QVariantAnimation* m_hoverAnimation = nullptr;
};
