#pragma once

#include <QFont>
#include <QFontMetrics>

class QApplication;
class QPainter;
class QString;

namespace AppFonts {

QFont applyPlatformDefaults(const QFont& font);
QFont loadFont(const QString& source, const QFont& fallback);
QFont defaultUiFont(const QFont& fallback);
QFont minecraftFont(const QFont& fallback);
QFont sizedFont(const QFont& base, qreal size, bool bold = false);
QFont pixelSizedFont(const QFont& base, int size, bool bold = false);
QFont applicationSizedFont(qreal size, bool bold = false);
QFont applicationPixelSizedFont(int size, bool bold = false);
QFont applicationPixelWeightedFont(int size, int weight);
QFontMetrics applicationSizedMetrics(qreal size, bool bold = false);
QFontMetrics applicationPixelSizedMetrics(int size, bool bold = false);
QFontMetrics applicationPixelWeightedMetrics(int size, int weight);
void configureApplicationFont(QApplication& app);
void configurePainterForText(QPainter& painter);

} // namespace AppFonts
