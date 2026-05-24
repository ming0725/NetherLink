#pragma once

#include <QImage>
#include <QString>
#include <QWidget>

#include <functional>

class QMouseEvent;
class QPaintEvent;

class ProfileAvatarPreview final : public QWidget
{
public:
    explicit ProfileAvatarPreview(QWidget* parent = nullptr);

    void setAvatarSource(const QString& source);

    std::function<void(const QImage&)> avatarImageSelected;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QString m_avatarSource;
};
