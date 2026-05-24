#pragma once

#include <QColor>
#include <QObject>
#include <QPalette>
#include <QPointer>
#include <QString>

class QApplication;
class QEvent;

enum class ThemeColor {
    // Accent-derived roles. These should follow the user-selected theme color.
    Accent,
    AccentHover,
    AccentPressed,
    AccentBubble,
    AccentLinkText,
    AccentLinkTextOnAccent,
    AccentTextSelection,
    AccentTextSelectionOnAccent,
    TextOnAccent,
    SecondaryTextOnAccent,
    ListSelected,

    // Neutral and fixed semantic roles. These follow light/dark mode or
    // their own semantic color, not the user-selected theme color.
    WindowBackground,
    PageBackground,
    PanelBackground,
    PanelRaisedBackground,
    InputBackground,
    InputFocusBackground,
    PrimaryText,
    SecondaryText,
    TertiaryText,
    PlaceholderText,
    Divider,
    ListHover,
    ListPinned,
    ChatInfoPanelOverlay,
    ChatInfoPanelFallbackBackground,
    ChatInfoPanelText,
    ChatInfoPanelCardBackground,
    ChatInfoPanelCardHover,
    ChatInfoPanelCardPressed,
    CreateGroupPopupBackground,
    CreateGroupPopupDivider,
    CreateGroupPopupHover,
    CreateGroupPopupPrimaryText,
    CreateGroupPopupSecondaryText,
    CreateGroupPopupTertiaryText,
    SettingsOverlay,
    SettingsFallbackBackground,
    ChatInfoMemberListBackground,
    ChatInfoMemberListHover,
    ChatInfoMemberListPrimaryText,
    ChatInfoMemberListSecondaryText,
    ChatInfoMemberListHeaderText,
    DangerText,
    DangerControlHover,
    DangerControlPressed,
    AppBarItemBackground,
    AppBarItemSelectedBackground,
    PostBarItemSelectedBackground,
    ImagePlaceholder,
    LoadingPlaceholderBase,
    LoadingPlaceholderHighlight,
    LoadingPlaceholderLineBase,
    LoadingPlaceholderLineHighlight,
    ControlHover,
    ControlPressed,
    MessageBubblePeer,
    MessageBubblePeerSelected,
    ContextMenuBackground,
    ContextMenuBorder,
    ContextMenuHover,
    ContextMenuSeparator,
    ContextMenuText,
    ContextMenuDisabledText,
    ContextMenuShortcutText,
    PopupShadow,
    FloatingPanelShadow,
    NewMessageNotifierBackground,
    NewMessageNotifierHover,
    NewMessageNotifierPressed,
    NewMessageNotifierText,
    NewMessageNotifierShadow,
    BadgeUnreadBackground,
    BadgeUnreadText,
    BadgeMutedBackground,
    BadgeMutedText,
    BadgeSelectedBackground,
    RoleOwnerBackground,
    RoleAdminBackground,
    RoleOwnerText,
    RoleAdminText,
    DestructiveActionText,
    DestructiveActionBackground,
    NotificationFallbackBackground,
    NotificationTitleShadow,
    NotificationTitleText,
    NotificationBodyText,
    NotificationCloseBackground,
    NotificationCloseIcon,
    WindowBackdropTint,
    WindowCloseHover,
    OverlayStroke,
    InWindowPopupStroke,
    TooltipBackground,
    TooltipText,
    MediaOverlayStart,
    MediaOverlayEnd,
    PostOverlay,
    ScrollThumb
};

class ThemeManager final : public QObject
{
    Q_OBJECT
public:
    enum class Mode {
        Light = 1,
        Dark = 2,
        FollowSystem = 3
    };

    enum class QtFallbackInputBarEffect {
        Default = 0,
        LiquidGlass = 1,
        GaussianBlur = 2
    };

    enum class FontFamilyMode {
        Default = 0,
        Minecraft = 1
    };

    static ThemeManager& instance();

    Mode configuredMode() const;
    void setMode(Mode mode);
    bool isDark() const;
    QColor color(ThemeColor role) const;
    QColor postBarItemSelectedBackgroundColor() const;
    QColor themeColor() const;
    void setThemeColor(const QColor& color);
    QtFallbackInputBarEffect qtFallbackInputBarEffect() const;
    void setQtFallbackInputBarEffect(QtFallbackInputBarEffect effect);
    FontFamilyMode fontFamilyMode() const;
    void setFontFamilyMode(FontFamilyMode mode);
    bool qtFallbackLiquidGlassEnabled() const;
    void setQtFallbackLiquidGlassEnabled(bool enabled);
    bool postBarQtFallbackLiquidGlassEnabled() const;
    void setPostBarQtFallbackLiquidGlassEnabled(bool enabled);
    static QColor textColorOn(const QColor& background, int alpha = 255);
    static QColor colorCompositedOver(const QColor& foreground, const QColor& background);

    QPalette applicationPalette() const;
    void applyToApplication(QApplication& application);

signals:
    void themeChanged();

private:
    explicit ThemeManager(QObject* parent = nullptr);

    bool eventFilter(QObject* watched, QEvent* event) override;
    static bool isAccentColorRole(ThemeColor role);
    QColor accentColor(ThemeColor role, bool dark) const;
    QColor fixedColor(ThemeColor role, bool dark) const;
    void installSystemThemeListener(QApplication& application);
    void handleSystemColorSchemeChanged();
    void refreshApplicationTheme();
    bool systemIsDark() const;

    QPointer<QApplication> m_application;
    Mode m_configuredMode = Mode::FollowSystem;
    QColor m_themeColor = QColor(0x00, 0x99, 0xff);
    QtFallbackInputBarEffect m_qtFallbackInputBarEffect = QtFallbackInputBarEffect::LiquidGlass;
    FontFamilyMode m_fontFamilyMode = FontFamilyMode::Default;
    bool m_postBarQtFallbackLiquidGlassEnabled = true;
    bool m_refreshing = false;
};
