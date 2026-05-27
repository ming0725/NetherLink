#pragma once

#include <QtCore/QtGlobal>

class QWidget;

#ifdef Q_OS_WIN
namespace WindowsPopupChrome {

void applyModernShadow(QWidget* widget);

} // namespace WindowsPopupChrome
#endif
