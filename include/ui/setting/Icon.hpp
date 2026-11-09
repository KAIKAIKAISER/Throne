#pragma once

#include <QIcon>
#include <QString>

namespace Icon {

    enum class TrayIconStatus {
        None,
        Connecting,
        Running,
        SystemProxy,
        Vpn,
    };

    QIcon GetTrayIcon(TrayIconStatus status);

    QIcon GetTaskbarIcon(TrayIconStatus status);

    QIcon GetUiIcon(const QString &name);

    QString GetUiIconPath(const QString &name);

    // Forces the next GetTrayIcon to re-read the icon files; a use_custom_icons flip is detected without it.
    void InvalidateTrayIconCache();
} // namespace Icon
