#include "AppFonts.h"

#include <QApplication>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QHash>
#include <QPainter>
#include <QString>
#include <QStringList>

namespace {

const QString kDefaultFontSource(QStringLiteral(":/resources/font/MiSans-Regular.ttf"));
const QString kMinecraftFontSource(QStringLiteral(":/resources/font/MinecraftAE.ttf"));

QString fontCacheKey(const QFont& base, const QString& unit, qreal size, bool bold, int weight)
{
    return QStringLiteral("%1|%2|%3|%4|%5")
            .arg(base.toString(),
                 unit,
                 QString::number(size, 'f', 2),
                 bold ? QStringLiteral("1") : QStringLiteral("0"),
                 QString::number(weight));
}

QStringList preferredUiFamilies(const QString& bundledFallbackFamily,
                                const QString& currentFamily)
{
    QStringList families;
#ifdef Q_OS_MACOS
    families << QStringLiteral("PingFang SC");
    if (!bundledFallbackFamily.isEmpty()) {
        families << bundledFallbackFamily;
    }
#else
    if (!bundledFallbackFamily.isEmpty()) {
        families << bundledFallbackFamily;
    }
    families << QStringLiteral("PingFang SC");
#endif
    if (!currentFamily.isEmpty()) {
        families << currentFamily;
    }
    families << QStringLiteral("sans-serif");
    families.removeDuplicates();
    return families;
}

QString fontFamilyForSource(const QString& source)
{
    static QHash<QString, QString> familyCache;
    if (familyCache.contains(source)) {
        return familyCache.value(source);
    }

    const int fontId = QFontDatabase::addApplicationFont(source);
    if (fontId < 0) {
        familyCache.insert(source, QString());
        return {};
    }

    const QStringList families = QFontDatabase::applicationFontFamilies(fontId);
    const QString family = families.isEmpty() ? QString() : families.first();
    familyCache.insert(source, family);
    return family;
}

} // namespace

namespace AppFonts {

QFont applyPlatformDefaults(const QFont& font)
{
    QFont adjusted(font);
    adjusted.setKerning(true);
#ifdef Q_OS_WIN
    adjusted.setStyleStrategy(static_cast<QFont::StyleStrategy>(
            QFont::PreferAntialias | QFont::PreferOutline));
    adjusted.setHintingPreference(QFont::PreferNoHinting);
#endif
    return adjusted;
}

QFont loadFont(const QString& source, const QFont& fallback)
{
    QFont font = fallback;
    const QString family = fontFamilyForSource(source);
    if (!family.isEmpty()) {
        font.setFamily(family);
    }
    return applyPlatformDefaults(font);
}

QFont defaultUiFont(const QFont& fallback)
{
    QFont font = fallback;
    const QString bundledFamily = fontFamilyForSource(kDefaultFontSource);
#if QT_VERSION >= QT_VERSION_CHECK(5, 13, 0)
    font.setFamilies(preferredUiFamilies(bundledFamily, fallback.family()));
#else
    font.setFamily(bundledFamily.isEmpty() ? fallback.family() : bundledFamily);
#endif
    return applyPlatformDefaults(font);
}

QFont minecraftFont(const QFont& fallback)
{
    return loadFont(kMinecraftFontSource, fallback);
}

QFont sizedFont(const QFont& base, qreal size, bool bold)
{
    QFont font = applyPlatformDefaults(base);
    font.setPointSizeF(size);
    font.setBold(bold);
    return font;
}

QFont pixelSizedFont(const QFont& base, int size, bool bold)
{
    QFont font = applyPlatformDefaults(base);
    font.setPixelSize(size);
    font.setBold(bold);
    return font;
}

void configurePainterForText(QPainter& painter)
{
    painter.setRenderHint(QPainter::TextAntialiasing, true);
#if QT_VERSION >= QT_VERSION_CHECK(6, 1, 0)
    painter.setRenderHint(QPainter::VerticalSubpixelPositioning, true);
#endif
}

QFont applicationSizedFont(qreal size, bool bold)
{
    static QHash<QString, QFont> cache;
    const QFont base = QApplication::font();
    const QString key = fontCacheKey(base, QStringLiteral("pt"), size, bold, -1);
    const auto it = cache.constFind(key);
    if (it != cache.constEnd()) {
        return it.value();
    }

    const QFont font = sizedFont(base, size, bold);
    cache.insert(key, font);
    return font;
}

QFont applicationPixelSizedFont(int size, bool bold)
{
    static QHash<QString, QFont> cache;
    const QFont base = QApplication::font();
    const QString key = fontCacheKey(base, QStringLiteral("px"), size, bold, -1);
    const auto it = cache.constFind(key);
    if (it != cache.constEnd()) {
        return it.value();
    }

    const QFont font = pixelSizedFont(base, size, bold);
    cache.insert(key, font);
    return font;
}

QFont applicationPixelWeightedFont(int size, int weight)
{
    static QHash<QString, QFont> cache;
    const QFont base = QApplication::font();
    const QString key = fontCacheKey(base, QStringLiteral("pxw"), size, false, weight);
    const auto it = cache.constFind(key);
    if (it != cache.constEnd()) {
        return it.value();
    }

    QFont font = pixelSizedFont(base, size);
    font.setWeight(static_cast<QFont::Weight>(weight));
    cache.insert(key, font);
    return font;
}

QFontMetrics applicationSizedMetrics(qreal size, bool bold)
{
    static QHash<QString, QFontMetrics> cache;
    const QFont base = QApplication::font();
    const QString key = fontCacheKey(base, QStringLiteral("pt"), size, bold, -1);
    const auto it = cache.constFind(key);
    if (it != cache.constEnd()) {
        return it.value();
    }

    const QFontMetrics metrics(applicationSizedFont(size, bold));
    cache.insert(key, metrics);
    return metrics;
}

QFontMetrics applicationPixelSizedMetrics(int size, bool bold)
{
    static QHash<QString, QFontMetrics> cache;
    const QFont base = QApplication::font();
    const QString key = fontCacheKey(base, QStringLiteral("px"), size, bold, -1);
    const auto it = cache.constFind(key);
    if (it != cache.constEnd()) {
        return it.value();
    }

    const QFontMetrics metrics(applicationPixelSizedFont(size, bold));
    cache.insert(key, metrics);
    return metrics;
}

QFontMetrics applicationPixelWeightedMetrics(int size, int weight)
{
    static QHash<QString, QFontMetrics> cache;
    const QFont base = QApplication::font();
    const QString key = fontCacheKey(base, QStringLiteral("pxw"), size, false, weight);
    const auto it = cache.constFind(key);
    if (it != cache.constEnd()) {
        return it.value();
    }

    const QFontMetrics metrics(applicationPixelWeightedFont(size, weight));
    cache.insert(key, metrics);
    return metrics;
}

void configureApplicationFont(QApplication& app)
{
    app.setFont(defaultUiFont(app.font()));
}

} // namespace AppFonts
