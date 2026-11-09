#include "include/ui/setting/Icon.hpp"
#include "include/ui/setting/ThemeManager.hpp"
#include "include/global/Configs.hpp"

#include <QHash>
#include <QPixmap>
#include <QFile>

#include <optional>

namespace {
    QHash<Icon::TrayIconStatus, QIcon> g_trayIcons;
    std::optional<bool> g_trayIconsCustom;
    QString g_cachedTheme;

    QString statusName(Icon::TrayIconStatus status) {
        switch (status) {
            case Icon::TrayIconStatus::None: return QStringLiteral("Off");
            case Icon::TrayIconStatus::Connecting: return QStringLiteral("Connecting");
            case Icon::TrayIconStatus::Running: return QStringLiteral("Throne");
            case Icon::TrayIconStatus::SystemProxy: return QStringLiteral("Proxy");
            case Icon::TrayIconStatus::Vpn: return QStringLiteral("Tun");
        }
        MW_show_log("Icon::GetTrayIcon: Unknown status");
        return QStringLiteral("Off");
    }

    QIcon loadNamedIcon(const QString &name, bool useCustom, const QString &customFallback = {}) {
        // 1. Current third-party theme icons
        if (themeManager()) {
            QIcon themeIcon = themeManager()->getCustomIcon(name);
            if (!themeIcon.isNull()) return themeIcon;
            if (!customFallback.isEmpty()) {
                themeIcon = themeManager()->getCustomIcon(customFallback);
                if (!themeIcon.isNull()) return themeIcon;
            }
        }

        // 2. Legacy icons/ directory
        if (useCustom) {
            for (const auto &candidate : {name, customFallback}) {
                if (candidate.isEmpty()) continue;
                if (const auto custom = QPixmap(QStringLiteral("icons/") + candidate + QStringLiteral(".png")); !custom.isNull()) return QIcon(custom);
            }
        }

        // 3. Bundled resource
        return QIcon(QStringLiteral(":/Throne/") + name + QStringLiteral(".png"));
    }
} // namespace

void Icon::InvalidateTrayIconCache() {
    g_trayIcons.clear();
}

QIcon Icon::GetTrayIcon(TrayIconStatus status) {
    const bool useCustom = Configs::dataManager && Configs::dataManager->settingsRepo
                               ? Configs::dataManager->settingsRepo->use_custom_icons
                               : false;
    const QString curTheme = themeManager() ? themeManager()->current_theme : QString();
    if (g_trayIconsCustom != useCustom || g_cachedTheme != curTheme) {
        g_trayIcons.clear();
        g_trayIconsCustom = useCustom;
        g_cachedTheme = curTheme;
    }
    if (const auto it = g_trayIcons.constFind(status); it != g_trayIcons.cend()) return it.value();

    const QString customFallback = status == TrayIconStatus::Connecting ? statusName(TrayIconStatus::None) : QString();
    const QIcon icon = loadNamedIcon(statusName(status), useCustom, customFallback);
    g_trayIcons.insert(status, icon);
    return icon;
}

QIcon Icon::GetTaskbarIcon(TrayIconStatus status) {
    if (themeManager() && !themeManager()->currentThemeDir().isEmpty()) {
        return GetTrayIcon(status);
    }
    if (Configs::dataManager && Configs::dataManager->settingsRepo) {
        const auto &settings = Configs::dataManager->settingsRepo;
        if (settings->use_custom_icons && !settings->follow_status_in_taskbar) {
            return QIcon(QStringLiteral(":/Throne/Throne.png"));
        }
    }
    return GetTrayIcon(status);
}

QString Icon::GetUiIconPath(const QString &name) {
    if (themeManager()) {
        QString p = themeManager()->getCustomIconPath(name);
        if (!p.isEmpty()) return p;
    }
    return QString();
}

QIcon Icon::GetUiIcon(const QString &name) {
    // 1. Check third-party theme icons
    if (themeManager()) {
        QIcon icon = themeManager()->getCustomIcon(name);
        if (!icon.isNull()) return icon;
    }

    // Clean resource path if needed
    QString cleanName = name;
    if (cleanName.startsWith(QStringLiteral(":/icon/"))) cleanName = cleanName.mid(7);
    else if (cleanName.startsWith(QStringLiteral(":/Throne/"))) cleanName = cleanName.mid(9);
    else if (cleanName.startsWith(QStringLiteral(":/"))) cleanName = cleanName.mid(2);
    if (cleanName.endsWith(QStringLiteral(".png"))) cleanName.chop(4);

    // 2. Legacy icons/ directory
    if (Configs::dataManager && Configs::dataManager->settingsRepo && Configs::dataManager->settingsRepo->use_custom_icons) {
        if (const auto custom = QPixmap(QStringLiteral("icons/") + cleanName + QStringLiteral(".png")); !custom.isNull()) {
            return QIcon(custom);
        }
    }

    // 3. Fallback bundled resources
    if (QFile::exists(QStringLiteral(":/icon/") + cleanName + QStringLiteral(".png"))) {
        return QIcon(QStringLiteral(":/icon/") + cleanName + QStringLiteral(".png"));
    }
    if (QFile::exists(QStringLiteral(":/icon/material/") + cleanName + QStringLiteral(".png"))) {
        return QIcon(QStringLiteral(":/icon/material/") + cleanName + QStringLiteral(".png"));
    }
    if (QFile::exists(QStringLiteral(":/Throne/") + cleanName + QStringLiteral(".png"))) {
        return QIcon(QStringLiteral(":/Throne/") + cleanName + QStringLiteral(".png"));
    }
    if (QFile::exists(name)) {
        return QIcon(name);
    }
    return QIcon();
}
