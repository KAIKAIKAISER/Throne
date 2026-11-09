#pragma once

#include <QObject>
#include <QPalette>
#include <QColor>
#include <QHash>
#include <QIcon>
#include <QStyle>
#include <QStringList>
#include <QMap>

#include <array>

// UI colours resolved from the installed palette, then contrast-repaired against `surface`.
// Use these rather than palette(...) in a QSS: stylesheet palette() resolves against
// QGuiApplication::palette() regardless of the widget, and only ever the Active group.
struct ThemeTokens {
    QColor surface;
    QColor onSurface;
    QColor accent;
    QColor muted;
    QColor tag;
    QColor danger;
    QColor success;
    QColor info;
    std::array<QColor, 5> chartSeries;
    QColor chartContext;
    // Custom themes only; invalid under a Qt style, which keeps the Link-coloured running row.
    QColor runningRow;
    QColor onRunningRow;
    QColor runningMark;
    QColor onRunningMark;
};

struct ThemeColors {
    QColor window, windowText;
    QColor base, alternateBase;
    QColor text;
    QColor button, buttonText;
    QColor brightText;
    QColor highlight, highlightedText;
    QColor link;            // also the running row's header mark
    QColor tooltipBase, tooltipText;
    QColor placeholder;
    QColor disabledText;
    QColor runningRow;      // between the sheet's row stripes and its selection fill
};

struct CustomThemeInfo {
    QString id;
    QString name;
    QString author;
    QString version;
    QString description;
    QString dirPath;        // Directory containing the theme files
    QString qssPath;        // Path to the primary .qss / .css file
    QString baseStyle;      // Base QStyle (e.g., "windows11", "Fusion", "windows")
    bool isBuiltIn = false;
    bool isDark = false;
    bool hasColors = false;
    ThemeColors colors;
};

class ThemeManager : public QObject {
    Q_OBJECT
public:
    QString system_style_name = "";
    QPalette system_palette;     // snapshot of the OS palette, taken on first apply
    QString current_theme = "0"; // int: 0:system 1+:builtin string: QStyleFactory or custom name
    ThemeTokens tokens;          // rebuilt by every ApplyTheme(), before themeChanged fires

    static QString themesDirectory();

    void loadAvailableThemes();
    QStringList availableCustomThemeNames() const;
    const CustomThemeInfo* getThemeInfo(const QString &themeName) const;
    bool isCustomTheme(const QString &themeName) const;
    bool isCurrentThemeDark() const;

    QString currentThemeDir() const { return current_theme_dir; }
    QIcon getCustomIcon(const QString &nameOrPath) const;
    QString getCustomIconPath(const QString &nameOrPath) const;

    bool importThemeFromZip(const QString &zipFilePath, QString &outThemeName, QString &errorMsg);
    bool deleteTheme(const QString &themeName, QString &errorMsg);

    void ApplyTheme(const QString &theme, bool force = false);

    // QStyle::standardIcon loads from the system on every call: about 9 ms for a message box icon on Windows.
    QIcon standardIcon(QStyle::StandardPixmap pixmap);

signals:
    void themeChanged(QString themeName);
    void themesListUpdated();

private:
    QHash<int, QIcon> standardIcons; // the current style's; ApplyTheme() reloads the message box icons
    bool applying = false;
    QString current_theme_dir;
    QMap<QString, CustomThemeInfo> m_customThemes;
};

ThemeManager *themeManager();
