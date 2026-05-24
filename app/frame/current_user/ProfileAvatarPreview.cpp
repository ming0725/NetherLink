#include "app/frame/current_user/ProfileAvatarPreview.h"

#include "app/frame/current_user/CurrentUserPopupStyle.h"
#include "shared/services/AudioService.h"
#include "shared/services/ImageService.h"
#include "shared/theme/ThemeManager.h"

#include <QDir>
#include <QFileDialog>
#include <QImageReader>
#include <QMouseEvent>
#include <QPainter>
#include <QStandardPaths>

using namespace CurrentUserPopupStyle;

ProfileAvatarPreview::ProfileAvatarPreview(QWidget* parent)
    : QWidget(parent)
{
    setFixedSize(kProfileEditAvatarSize, kProfileEditAvatarSize);
    setCursor(Qt::PointingHandCursor);
}

void ProfileAvatarPreview::setAvatarSource(const QString& source)
{
    m_avatarSource = source;
    update();
}

void ProfileAvatarPreview::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform, true);
    const QRect avatarRect = rect();
    const QPixmap avatar = ImageService::instance().circularAvatar(m_avatarSource,
                                                                   kProfileEditAvatarSize,
                                                                   devicePixelRatioF());
    painter.setPen(Qt::NoPen);
    if (!avatar.isNull()) {
        painter.drawPixmap(avatarRect, avatar);
    } else {
        painter.setBrush(ThemeManager::instance().color(ThemeColor::ImagePlaceholder));
        painter.drawEllipse(avatarRect);
    }
    if (underMouse()) {
        painter.setBrush(QColor(0, 0, 0, 80));
        painter.drawEllipse(avatarRect);
    }
}

void ProfileAvatarPreview::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && rect().contains(event->pos())) {
        AudioService::instance().playButtonClick();
        QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
        if (defaultPath.isEmpty()) {
            defaultPath = QDir::homePath();
        }
        const QString filePath = QFileDialog::getOpenFileName(
                this,
                QStringLiteral("选择头像"),
                defaultPath,
                QStringLiteral("图片文件 (*.png *.jpg *.jpeg *.bmp *.webp);;所有文件 (*)"));
        if (!filePath.isEmpty()) {
            QImageReader reader(filePath);
            reader.setAutoTransform(true);
            const QImage image = reader.read();
            if (!image.isNull() && avatarImageSelected) {
                avatarImageSelected(image);
            }
        }
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}
