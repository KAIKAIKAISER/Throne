#include <QStyle>
#include <QApplication>
#include <QFile>
#include <QFont>
#include <QPalette>
#include <QColor>
#include <QMap>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QDateTime>
#include <QScopedValueRollback>

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
#include <QStyleHints>
#endif

#ifdef Q_OS_MACOS
#include <QEvent>
#include <QFormLayout>
#include <QWidget>
#endif

#include <algorithm>
#include <cmath>

#include "include/ui/setting/ThemeManager.hpp"
#include "include/ui/setting/Icon.hpp"
#include "include/ui/utils/ZipUtils.hpp"
#include "include/global/Configs.hpp"

#include <QGlobalStatic>

Q_GLOBAL_STATIC(ThemeManager, themeManagerInstance)

ThemeManager *themeManager() {
    return themeManagerInstance();
}

extern QString ReadFileText(const QString &path);

static QPalette buildThemePalette(const ThemeColors &c) {
    QPalette p;

    const auto setAll = [&](QPalette::ColorRole role, const QColor &col) {
        p.setColor(QPalette::Active, role, col);
        p.setColor(QPalette::Inactive, role, col);
        p.setColor(QPalette::Disabled, role, col);
    };

    setAll(QPalette::Window,          c.window);
    setAll(QPalette::WindowText,      c.windowText);
    setAll(QPalette::Base,            c.base);
    setAll(QPalette::AlternateBase,   c.alternateBase);
    setAll(QPalette::Text,            c.text);
    setAll(QPalette::Button,          c.button);
    setAll(QPalette::ButtonText,      c.buttonText);
    setAll(QPalette::BrightText,      c.brightText);
    setAll(QPalette::ToolTipBase,     c.tooltipBase);
    setAll(QPalette::ToolTipText,     c.tooltipText);
    setAll(QPalette::Highlight,       c.highlight);
    setAll(QPalette::HighlightedText, c.highlightedText);
    setAll(QPalette::Link,            c.link);
    setAll(QPalette::LinkVisited,     c.link);
    setAll(QPalette::PlaceholderText, c.placeholder);

    // Frames and bevels the stylesheet doesn't cover fall back to Qt's light defaults otherwise.
    setAll(QPalette::Light,    c.button.lighter(130));
    setAll(QPalette::Midlight, c.button.lighter(115));
    setAll(QPalette::Mid,      c.button.darker(130));
    setAll(QPalette::Dark,     c.button.darker(160));
    setAll(QPalette::Shadow,   c.window.darker(180));

    // Must follow setAll(), which wrote the Disabled group too.
    p.setColor(QPalette::Disabled, QPalette::WindowText,      c.disabledText);
    p.setColor(QPalette::Disabled, QPalette::Text,            c.disabledText);
    p.setColor(QPalette::Disabled, QPalette::ButtonText,      c.disabledText);
    p.setColor(QPalette::Disabled, QPalette::HighlightedText, c.disabledText);
    p.setColor(QPalette::Disabled, QPalette::Link,            c.disabledText);

    return p;
}

// Built-in custom themes.
static const QMap<QString, ThemeColors> &builtInThemes() {
    static const QMap<QString, ThemeColors> themes = [] {
        QMap<QString, ThemeColors> m;

        m["flatgray"] = {
            .window = "#FFFFFF", .windowText = "#57595B",
            .base = "#FFFFFF", .alternateBase = "#F6F6F6",
            .text = "#57595B",
            .button = "#F2F2F2", .buttonText = "#57595B",
            .brightText = "#FFFFFF",
            .highlight = "#D6D6D6", .highlightedText = "#2D2F31",
            .link = "#2A6CB0",
            .tooltipBase = "#FFFFFF", .tooltipText = "#57595B",
            .placeholder = "#9AA0A6", .disabledText = "#B0B0B0",
            .runningRow = "#CADAEB",
        };

        m["lightblue"] = {
            .window = "#EAF7FF", .windowText = "#386487",
            .base = "#FFFFFF", .alternateBase = "#DAEFFF",
            .text = "#386487",
            .button = "#DEF0FE", .buttonText = "#386487",
            .brightText = "#FFFFFF",
            .highlight = "#C0DCF2", .highlightedText = "#1B3B57",
            .link = "#1D6FB8",
            .tooltipBase = "#EAF7FF", .tooltipText = "#386487",
            .placeholder = "#7F9DB5", .disabledText = "#A6BCCE",
            .runningRow = "#B7D5ED",
        };

        m["softpink"] = {
            .window = "#FFF0FB", .windowText = "#883983",
            .base = "#FFFFFF", .alternateBase = "#FBDDF5",
            .text = "#883983",
            .button = "#FCE1F6", .buttonText = "#883983",
            .brightText = "#FFFFFF",
            .highlight = "#F1C1E7", .highlightedText = "#5A2456",
            .link = "#B92BA6",
            .tooltipBase = "#FFF0FB", .tooltipText = "#883983",
            .placeholder = "#C08BBA", .disabledText = "#CBA6C6",
            .runningRow = "#EEBFE6",
        };

        m["blacksoft"] = {
            .window = "#444444", .windowText = "#DCDCDC",
            .base = "#444444", .alternateBase = "#525252",
            .text = "#DCDCDC",
            .button = "#484848", .buttonText = "#DCDCDC",
            .brightText = "#FFFFFF",
            .highlight = "#646464", .highlightedText = "#FFFFFF",
            .link = "#5AB0FF",
            .tooltipBase = "#484848", .tooltipText = "#DCDCDC",
            .placeholder = "#9A9A9A", .disabledText = "#808080",
            .runningRow = "#4B647C",
        };

        // Mirrors the bundled darkstyle.qss.
        m["qdarkstyle"] = {
            .window = "#19232D", .windowText = "#DFE1E2",
            .base = "#19232D", .alternateBase = "#37414F",
            .text = "#DFE1E2",
            .button = "#455364", .buttonText = "#DFE1E2",
            .brightText = "#FFFFFF",
            .highlight = "#346792", .highlightedText = "#DFE1E2",
            .link = "#6FC0FF",
            .tooltipBase = "#346792", .tooltipText = "#DFE1E2",
            .placeholder = "#9DA9B5", .disabledText = "#788D9C",
            .runningRow = "#2E4A62",
        };

        return m;
    }();
    return themes;
}

static double relLuminance(const QColor &c) {
    const auto channel = [](double v) {
        v /= 255.0;
        return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * channel(c.red()) + 0.7152 * channel(c.green()) + 0.0722 * channel(c.blue());
}

static double contrastRatio(const QColor &a, const QColor &b) {
    const double la = relLuminance(a), lb = relLuminance(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

static QColor blendToward(const QColor &from, const QColor &to, double keep) {
    return QColor::fromRgbF(from.redF()   * keep + to.redF()   * (1 - keep),
                            from.greenF() * keep + to.greenF() * (1 - keep),
                            from.blueF()  * keep + to.blueF()  * (1 - keep));
}

// Walks HSL lightness away from `surface` until the ratio is met; hue and saturation survive.
static QColor separate(QColor c, const QColor &surface, double target) {
    const int dir = relLuminance(surface) > 0.5 ? -1 : 1;
    for (int i = 0; i < 24 && contrastRatio(c, surface) < target; ++i) {
        int h, s, l, a;
        c.getHsl(&h, &s, &l, &a);
        const int next = qBound(0, l + dir * 10, 255);
        if (next == l) break;
        c.setHsl(h < 0 ? 0 : h, s, next, a);
    }
    return c;
}

static QColor paletteAccent(const QPalette &pal) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    return pal.color(QPalette::Active, QPalette::Accent);
#else
    return pal.color(QPalette::Active, QPalette::Highlight);
#endif
}

static QColor selectedFill(const QPalette &pal, const QColor &surface, const QColor &onSurface,
                           const QColor &accent) {
    for (const auto role : {QPalette::AlternateBase, QPalette::Base, QPalette::Button, QPalette::Midlight}) {
        const QColor c = pal.color(QPalette::Active, role);
        if (contrastRatio(c, surface) >= 1.35 && contrastRatio(onSurface, c) >= 4.5) return c;
    }
    return blendToward(accent, separate(surface, surface, 1.5), 0.22);
}

static ThemeTokens resolveTokens(const QPalette &pal) {
    ThemeTokens t;
    t.surface   = pal.color(QPalette::Active, QPalette::Window);
    t.onSurface = pal.color(QPalette::Active, QPalette::WindowText);

    t.accent  = separate(paletteAccent(pal), t.surface, 3.0);
    t.muted   = separate(blendToward(t.onSurface, t.surface, 0.62), t.surface, 4.0);
    t.tag     = separate(QColor(0xFB, 0x72, 0x99), t.surface, 4.0);
    t.danger  = separate(QColor(0xC6, 0x28, 0x28), t.surface, 4.5);
    t.success = separate(QColor(0x2E, 0x7D, 0x32), t.surface, 4.5);
    t.info    = separate(QColor(0x32, 0x99, 0xFF), t.surface, 4.0);
    const std::array<QColor, 5> series{QColor(0x2A, 0x78, 0xD6), QColor(0xEB, 0x68, 0x34), QColor(0x1B, 0xAF, 0x7A),
                                       QColor(0xE0, 0x69, 0x9A), QColor(0x5B, 0x4B, 0xC4)};
    for (size_t i = 0; i < series.size(); ++i) t.chartSeries[i] = separate(series[i], t.surface, 3.0);
    t.chartContext = separate(QColor(0x7D, 0x87, 0x96), t.surface, 3.0);
    return t;
}

static QString overlayStyleSheet(const ThemeTokens &t) {
    const auto hex = [](const QColor &c) { return c.name(QColor::HexRgb); };
    QString sheet = QStringLiteral(
        "*[colorRole=\"muted\"] { color: %1; }\n"
        "*[colorRole=\"tag\"] { color: %2; }\n"
        "*[colorRole=\"danger\"] { color: %3; }\n"
        "*[colorRole=\"success\"] { color: %4; }\n"
        "QLineEdit[invalid=\"true\"], QLineEdit[invalid=\"true\"]:focus, QLineEdit[invalid=\"true\"]:hover,\n"
        "QPlainTextEdit[invalid=\"true\"], QTextEdit[invalid=\"true\"],\n"
        "QAbstractSpinBox[invalid=\"true\"], QAbstractSpinBox[invalid=\"true\"]:focus, QAbstractSpinBox[invalid=\"true\"]:hover { color: %3; }\n"
        "QComboBox[invalid=\"true\"]:editable, QComboBox[invalid=\"true\"]:editable:focus, QComboBox[invalid=\"true\"]:editable:hover,\n"
        "QComboBox[invalid=\"true\"]:read-only, QComboBox[invalid=\"true\"]:read-only:focus, QComboBox[invalid=\"true\"]:read-only:hover,\n"
        "QComboBox[invalid=\"true\"]:!editable:!active { color: %3; }\n"
    ).arg(hex(t.muted), hex(t.tag), hex(t.danger), hex(t.success));
#ifdef Q_OS_MACOS
    sheet += QStringLiteral(
        "QTabWidget::tab-bar { alignment: left; }\n"
        "QTabBar { tabbar-prefer-no-arrows: 0; tabbar-elide-mode: %1; }\n"
    ).arg(int(Qt::ElideNone));
#endif
    return sheet;
}

static QColor paneBorder(const ThemeTokens &t) {
    return separate(blendToward(t.onSurface, t.surface, 0.32), t.surface, 1.9);
}

static QString windows11TabStyleSheet(const QPalette &pal, const ThemeTokens &t) {
    const auto hex = [](const QColor &c) { return c.name(QColor::HexRgb); };
    const QColor border = paneBorder(t);
    const QColor hover = separate(blendToward(t.accent, t.surface, 0.10), t.surface, 1.10);
    QColor selected = selectedFill(pal, t.surface, t.onSurface, t.accent);
    for (int i = 0; i < 8 && contrastRatio(t.onSurface, selected) < 4.5; ++i) {
        selected = blendToward(selected, t.surface, 0.6);
    }
    return QStringLiteral(
        "QTabWidget::pane { margin-top: 1px; border: 1px solid %1; border-radius: 4px; background: %7; }\n"
        "#profilesTableView, #masterLogBrowser, #connections { border: none; }\n"
        "QTabBar { background: transparent; qproperty-drawBase: 0; }\n"
        "QTabBar::tab {\n"
        "    background: transparent;\n"
        "    color: %2;\n"
        "    border: 1px solid %1;\n"
        "    border-radius: 4px;\n"
        "    padding: 2px 6px;\n"
        "    margin-right: 1px;\n"
        "}\n"
        "QTabBar::tab:hover:!selected { background: %3; }\n"
        "QTabBar::tab:selected { background: %4; color: %2; border: 1px solid %5; }\n"
        "QTabBar::tab:disabled { color: %6; }\n"
    ).arg(hex(border), hex(t.onSurface), hex(hover), hex(selected), hex(t.accent), hex(t.muted),
          hex(pal.color(QPalette::Active, QPalette::Base)));
}

static QString macPaneStyleSheet(const ThemeTokens &t) {
    return QStringLiteral(
        "QTabWidget::pane { border: 1px solid %1; border-radius: 6px; }\n"
        "#profilesTableView, #masterLogBrowser, #connections { border: none; }\n"
    ).arg(paneBorder(t).name(QColor::HexRgb));
}

#ifdef Q_OS_MACOS
class UniformFormLayouts : public QObject {
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (event->type() == QEvent::Polish && watched->isWidgetType()) {
            if (auto *layout = static_cast<QWidget *>(watched)->layout()) {
                normalize(qobject_cast<QFormLayout *>(layout));
                for (auto *form : layout->findChildren<QFormLayout *>()) normalize(form);
            }
        }
        return false;
    }

private:
    static void normalize(QFormLayout *form) {
        if (!form) return;
        if (form->fieldGrowthPolicy() == QFormLayout::FieldsStayAtSizeHint)
            form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        if (form->formAlignment() == (Qt::AlignHCenter | Qt::AlignTop))
            form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
        if (form->labelAlignment() == Qt::AlignRight)
            form->setLabelAlignment(Qt::AlignLeft);
    }
};
#endif

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
static void requestColorScheme(Qt::ColorScheme scheme) {
    static auto requested = Qt::ColorScheme::Unknown;
    if (scheme == requested) return;
    requested = scheme;
    qApp->styleHints()->setColorScheme(scheme);
}
#endif

// Rewrites relative url(...) paths in stylesheets into absolute paths based on theme directory.
static QString rewriteQssUrls(const QString &qss, const QString &themeDir) {
    if (themeDir.isEmpty()) return qss;
    QString normDir = QDir::fromNativeSeparators(themeDir);
    if (!normDir.endsWith('/')) normDir += '/';

    static const QRegularExpression urlRegex(QStringLiteral("url\\(\\s*([\"']?)([^)\"']+)\\1\\s*\\)"));
    QString result;
    int lastPos = 0;
    auto it = urlRegex.globalMatch(qss);
    while (it.hasNext()) {
        auto match = it.next();
        result.append(qss.mid(lastPos, match.capturedStart() - lastPos));
        QString path = match.captured(2).trimmed();
        if (path.startsWith(":/") || path.startsWith("http://") || path.startsWith("https://") ||
            path.startsWith('/') || (path.length() > 2 && path[1] == ':')) {
            result.append(match.captured(0));
        } else {
            QString fullPath = normDir + path;
            result.append(QStringLiteral("url(\"%1\")").arg(fullPath));
        }
        lastPos = match.capturedEnd();
    }
    result.append(qss.mid(lastPos));
    return result;
}

QString ThemeManager::themesDirectory() {
    QString base = Configs::GetBasePath();
    return QDir(base).filePath(QStringLiteral("themes"));
}

void ThemeManager::loadAvailableThemes() {
    m_customThemes.clear();

    // 1. Built-in custom themes
    for (const auto &[key, colors] : builtInThemes().asKeyValueRange()) {
        CustomThemeInfo info;
        info.id = key;
        info.name = key;
        info.isBuiltIn = true;
        info.hasColors = true;
        info.colors = colors;
        info.isDark = colors.windowText.lightness() > colors.window.lightness();
        m_customThemes.insert(key.toLower(), info);
    }

    // 2. Discover third-party themes in themes directory
    QStringList searchDirs;
    searchDirs << themesDirectory();
    QString appThemes = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("themes"));
    if (appThemes != themesDirectory() && !searchDirs.contains(appThemes)) {
        searchDirs << appThemes;
    }

    for (const auto &basePath : searchDirs) {
        QDir dir(basePath);
        if (!dir.exists()) continue;

        const auto subDirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const auto &subDirInfo : subDirs) {
            QString folderPath = subDirInfo.filePath();
            QDir themeDir(folderPath);

            CustomThemeInfo info;
            info.id = subDirInfo.fileName().toLower();
            info.name = subDirInfo.fileName();
            info.dirPath = folderPath;
            info.isBuiltIn = false;

            // Check for theme.json
            QString jsonPath = themeDir.filePath(QStringLiteral("theme.json"));
            if (QFile::exists(jsonPath)) {
                QFile f(jsonPath);
                if (f.open(QIODevice::ReadOnly)) {
                    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
                    if (doc.isObject()) {
                        QJsonObject root = doc.object();
                        if (root.contains("name")) info.name = root.value("name").toString();
                        if (root.contains("id")) info.id = root.value("id").toString().toLower();
                        if (root.contains("author")) info.author = root.value("author").toString();
                        if (root.contains("version")) info.version = root.value("version").toString();
                        if (root.contains("description")) info.description = root.value("description").toString();
                        if (root.contains("base_style")) info.baseStyle = root.value("base_style").toString();
                        if (root.contains("dark")) info.isDark = root.value("dark").toBool();

                        if (root.contains("style")) {
                            QString s = root.value("style").toString();
                            if (QFile::exists(themeDir.filePath(s))) info.qssPath = themeDir.filePath(s);
                        } else if (root.contains("qss")) {
                            QString s = root.value("qss").toString();
                            if (QFile::exists(themeDir.filePath(s))) info.qssPath = themeDir.filePath(s);
                        }

                        if (root.contains("colors") && root.value("colors").isObject()) {
                            QJsonObject c = root.value("colors").toObject();
                            auto col = [&](const char *key, const QColor &fallback = QColor()) -> QColor {
                                if (c.contains(key)) return QColor(c.value(key).toString());
                                return fallback;
                            };
                            info.hasColors = true;
                            info.colors.window = col("window", Qt::white);
                            info.colors.windowText = col("windowText", Qt::black);
                            info.colors.base = col("base", Qt::white);
                            info.colors.alternateBase = col("alternateBase", QColor(0xf0, 0xf0, 0xf0));
                            info.colors.text = col("text", Qt::black);
                            info.colors.button = col("button", QColor(0xe0, 0xe0, 0xe0));
                            info.colors.buttonText = col("buttonText", Qt::black);
                            info.colors.brightText = col("brightText", Qt::white);
                            info.colors.highlight = col("highlight", QColor(0x3a, 0x8d, 0xff));
                            info.colors.highlightedText = col("highlightedText", Qt::white);
                            info.colors.link = col("link", QColor(0x1d, 0x6f, 0xb8));
                            info.colors.tooltipBase = col("tooltipBase", info.colors.window);
                            info.colors.tooltipText = col("tooltipText", info.colors.windowText);
                            info.colors.placeholder = col("placeholder", QColor(0x9a, 0x9a, 0x9a));
                            info.colors.disabledText = col("disabledText", QColor(0x80, 0x80, 0x80));
                            info.colors.runningRow = col("runningRow", info.colors.alternateBase);

                            if (!root.contains("dark")) {
                                info.isDark = info.colors.windowText.lightness() > info.colors.window.lightness();
                            }
                        }
                    }
                }
            }

            // Find QSS if not specified
            if (info.qssPath.isEmpty()) {
                QStringList qssCandidates = {
                    QStringLiteral("style.qss"), QStringLiteral("theme.qss"),
                    QStringLiteral("style.css"), QStringLiteral("theme.css"),
                    info.name + QStringLiteral(".qss"), info.id + QStringLiteral(".qss")
                };
                for (const auto &cand : qssCandidates) {
                    if (themeDir.exists(cand)) {
                        info.qssPath = themeDir.filePath(cand);
                        break;
                    }
                }
                if (info.qssPath.isEmpty()) {
                    const auto allQss = themeDir.entryList(QStringList() << "*.qss" << "*.css", QDir::Files);
                    if (!allQss.isEmpty()) {
                        info.qssPath = themeDir.filePath(allQss.first());
                    }
                }
            }

            // A folder is a valid theme if it contains a stylesheet, a theme.json, or custom icons
            bool hasIcons = themeDir.exists(QStringLiteral("icons")) ||
                            !themeDir.entryList(QStringList() << "*.png" << "*.svg", QDir::Files).isEmpty();

            if (!info.qssPath.isEmpty() || info.hasColors || hasIcons) {
                m_customThemes.insert(info.name.toLower(), info);
                m_customThemes.insert(info.id.toLower(), info);
            }
        }
    }

    emit themesListUpdated();
}

QStringList ThemeManager::availableCustomThemeNames() const {
    QStringList names;
    for (const auto &info : m_customThemes.values()) {
        if (!info.isBuiltIn && !names.contains(info.name)) {
            names.append(info.name);
        }
    }
    return names;
}

const CustomThemeInfo* ThemeManager::getThemeInfo(const QString &themeName) const {
    auto it = m_customThemes.constFind(themeName.toLower());
    if (it != m_customThemes.constEnd()) {
        return &it.value();
    }
    return nullptr;
}

bool ThemeManager::isCustomTheme(const QString &themeName) const {
    const auto *info = getThemeInfo(themeName);
    return info && !info->isBuiltIn;
}

bool ThemeManager::isCurrentThemeDark() const {
    const auto *info = getThemeInfo(current_theme);
    if (info) return info->isDark;
    const auto lower = current_theme.toLower();
    if (lower.contains("vista") || lower.contains("flatgray") || lower.contains("lightblue") || lower.contains("softpink")) {
        return false;
    }
    if (lower.contains("dark") || lower.contains("blacksoft")) {
        return true;
    }
    return tokens.onSurface.lightness() > tokens.surface.lightness();
}

QString ThemeManager::getCustomIconPath(const QString &nameOrPath) const {
    if (current_theme_dir.isEmpty()) return QString();

    QString cleanName = nameOrPath;
    if (cleanName.startsWith(QStringLiteral(":/icon/"))) cleanName = cleanName.mid(7);
    else if (cleanName.startsWith(QStringLiteral(":/Throne/"))) cleanName = cleanName.mid(9);
    else if (cleanName.startsWith(QStringLiteral(":/"))) cleanName = cleanName.mid(2);

    QStringList candidates;
    auto addVariants = [&](const QString &base) {
        if (!base.endsWith(QStringLiteral(".png")) && !base.endsWith(QStringLiteral(".svg"))) {
            candidates << base + QStringLiteral(".png");
            candidates << base + QStringLiteral(".svg");
        } else {
            candidates << base;
        }
    };

    addVariants(cleanName);
    if (cleanName.startsWith(QStringLiteral("material/"))) {
        addVariants(cleanName.mid(9));
    } else {
        addVariants(QStringLiteral("material/") + cleanName);
    }
    if (cleanName.startsWith(QStringLiteral("b-"))) {
        addVariants(cleanName.mid(2));
    } else {
        addVariants(QStringLiteral("b-") + cleanName);
    }

    QDir themeDir(current_theme_dir);
    for (const auto &cand : candidates) {
        QString p1 = themeDir.filePath(QStringLiteral("icons/") + cand);
        if (QFile::exists(p1)) return p1;
        QString p2 = themeDir.filePath(cand);
        if (QFile::exists(p2)) return p2;
    }

    return QString();
}

QIcon ThemeManager::getCustomIcon(const QString &nameOrPath) const {
    QString path = getCustomIconPath(nameOrPath);
    if (!path.isEmpty()) {
        QPixmap pm(path);
        if (!pm.isNull()) return QIcon(pm);
    }
    return QIcon();
}

bool ThemeManager::importThemeFromZip(const QString &zipFilePath, QString &outThemeName, QString &errorMsg) {
    QFileInfo zipInfo(zipFilePath);
    if (!zipInfo.exists() || !zipInfo.isFile()) {
        errorMsg = tr("ZIP file does not exist: %1").arg(zipFilePath);
        return false;
    }

    QString tempExtractPath = QDir(QDir::tempPath()).filePath(
        QStringLiteral("throne_theme_%1").arg(QDateTime::currentMSecsSinceEpoch()));

    if (!ZipUtils::extractZip(zipFilePath, tempExtractPath, errorMsg)) {
        QDir(tempExtractPath).removeRecursively();
        return false;
    }

    // Inspect extracted contents
    QDir tempDir(tempExtractPath);
    const auto entries = tempDir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
    QString effectiveThemeDir = tempExtractPath;

    // If zip has a single root subfolder, drill down into it
    if (entries.size() == 1 && entries.first().isDir()) {
        effectiveThemeDir = entries.first().filePath();
    }

    QDir effDir(effectiveThemeDir);
    QString themeJsonPath = effDir.filePath(QStringLiteral("theme.json"));
    QStringList qssFiles = effDir.entryList(QStringList() << "*.qss" << "*.css", QDir::Files);
    bool hasIcons = effDir.exists(QStringLiteral("icons")) ||
                    !effDir.entryList(QStringList() << "*.png" << "*.svg", QDir::Files).isEmpty();

    if (!QFile::exists(themeJsonPath) && qssFiles.isEmpty() && !hasIcons) {
        errorMsg = tr("Invalid theme package: no theme.json, .qss stylesheet, or icons found in ZIP archive.");
        tempDir.removeRecursively();
        return false;
    }

    QString themeName;
    QString themeId;
    if (QFile::exists(themeJsonPath)) {
        QFile f(themeJsonPath);
        if (f.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
            if (doc.isObject()) {
                QJsonObject root = doc.object();
                if (root.contains("name")) themeName = root.value("name").toString();
                if (root.contains("id")) themeId = root.value("id").toString().toLower();
            }
        }
    }

    if (themeName.isEmpty()) {
        themeName = zipInfo.baseName();
    }
    if (themeId.isEmpty()) {
        themeId = themeName.toLower().replace(QRegularExpression(QStringLiteral("[^a-z0-9_\\-]")), QStringLiteral("_"));
    }

    QString destThemeDir = QDir(themesDirectory()).filePath(themeId);
    QDir destDir(destThemeDir);
    if (destDir.exists()) {
        destDir.removeRecursively();
    }
    QDir().mkpath(themesDirectory());

    bool success = ZipUtils::copyRecursively(effectiveThemeDir, destThemeDir);
    tempDir.removeRecursively();

    if (!success) {
        errorMsg = tr("Failed to copy theme to %1").arg(destThemeDir);
        return false;
    }

    loadAvailableThemes();
    outThemeName = themeName;
    return true;
}

bool ThemeManager::deleteTheme(const QString &themeName, QString &errorMsg) {
    const auto *info = getThemeInfo(themeName);
    if (!info || info->isBuiltIn || info->dirPath.isEmpty()) {
        errorMsg = tr("Cannot delete built-in theme.");
        return false;
    }

    QDir dir(info->dirPath);
    if (!dir.removeRecursively()) {
        errorMsg = tr("Failed to delete theme directory: %1").arg(info->dirPath);
        return false;
    }

    if (current_theme.toLower() == themeName.toLower()) {
        ApplyTheme(QStringLiteral("System"), true);
    }

    loadAvailableThemes();
    return true;
}

void ThemeManager::ApplyTheme(const QString &theme, bool force) {
    if (applying) return;
    const QScopedValueRollback guard(applying, true);

    if (this->system_style_name.isEmpty()) {
        this->system_style_name = qApp->style()->name();
        this->system_palette = qApp->palette();
#ifdef Q_OS_MACOS
        qApp->installEventFilter(new UniformFormLayouts(qApp));
#endif
        loadAvailableThemes();
    }

    if (this->current_theme == theme && !force) {
        return;
    }

    const auto lowerTheme = theme.toLower();
    const auto &builtins = builtInThemes();
    const auto *customInfo = getThemeInfo(theme);

    const bool wasCustom = !current_theme_dir.isEmpty() || builtins.contains(current_theme.toLower());
    const bool isBuiltinCustom = builtins.contains(lowerTheme);
    const bool isThirdParty = (customInfo && !customInfo->isBuiltIn);

    QString themeSheet;
    bool windows11Tabs = false;
    bool macosPane = false;

    if (isBuiltinCustom) {
        current_theme_dir = "";
        const ThemeColors colors = builtins.value(lowerTheme);
        qApp->setPalette(buildThemePalette(colors));
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        requestColorScheme(colors.windowText.lightness() > colors.window.lightness() ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light);
#endif
        themeSheet = lowerTheme == "qdarkstyle" ? ReadFileText(":/qdarkstyle/dark/darkstyle.qss")
                                                : ReadFileText(":/qss/" + lowerTheme + ".css");
    } else if (isThirdParty) {
        current_theme_dir = customInfo->dirPath;
        if (customInfo->hasColors) {
            qApp->setPalette(buildThemePalette(customInfo->colors));
        }
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        requestColorScheme(customInfo->isDark ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light);
#endif
        const QString baseStyle = customInfo->baseStyle.isEmpty() ? QStringLiteral("windows11") : customInfo->baseStyle;
        qApp->setStyle(baseStyle);
        windows11Tabs = baseStyle.compare(QStringLiteral("windows11"), Qt::CaseInsensitive) == 0;
        macosPane = baseStyle.compare(QStringLiteral("macos"), Qt::CaseInsensitive) == 0;

        if (!customInfo->qssPath.isEmpty()) {
            QString rawSheet = ReadFileText(customInfo->qssPath);
            themeSheet = rewriteQssUrls(rawSheet, customInfo->dirPath);
        }
    } else {
        current_theme_dir = "";
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        requestColorScheme(Qt::ColorScheme::Unknown);
#endif
        if (wasCustom) {
            qApp->setStyleSheet("");
            qApp->setPalette(system_palette);
        }
        const QString styleName = lowerTheme == "system" ? system_style_name : theme;
        qApp->setStyle(styleName);
        windows11Tabs = styleName.compare(QStringLiteral("windows11"), Qt::CaseInsensitive) == 0;
        macosPane = styleName.compare(QStringLiteral("macos"), Qt::CaseInsensitive) == 0;
    }

    tokens = resolveTokens(qApp->palette());
    if (isBuiltinCustom) {
        const ThemeColors colors = builtins.value(lowerTheme);
        tokens.runningRow = colors.runningRow;
        tokens.onRunningRow = separate(colors.link, colors.runningRow, 4.5);
        tokens.runningMark = colors.link;
        const QColor light = Qt::white, dark(0x1E, 0x1E, 0x1E);
        tokens.onRunningMark = contrastRatio(light, colors.link) >= contrastRatio(dark, colors.link) ? light : dark;
    } else if (isThirdParty && customInfo->hasColors) {
        tokens.runningRow = customInfo->colors.runningRow;
        tokens.onRunningRow = separate(customInfo->colors.link, customInfo->colors.runningRow, 4.5);
        tokens.runningMark = customInfo->colors.link;
        const QColor light = Qt::white, dark(0x1E, 0x1E, 0x1E);
        tokens.onRunningMark = contrastRatio(light, customInfo->colors.link) >= contrastRatio(dark, customInfo->colors.link) ? light : dark;
    }

    QString sheet = themeSheet + overlayStyleSheet(tokens);
    if (windows11Tabs) sheet += windows11TabStyleSheet(qApp->palette(), tokens);
    if (macosPane) sheet += macPaneStyleSheet(tokens);

    qApp->setStyleSheet(sheet);
    qApp->setFont(qApp->font());
    qApp->setStyleSheet(sheet);

    current_theme = theme;
    standardIcons.clear();
    Icon::InvalidateTrayIconCache();

    for (const auto pixmap : {QStyle::SP_MessageBoxCritical, QStyle::SP_MessageBoxWarning}) standardIcon(pixmap);

    emit themeChanged(theme);
}

QIcon ThemeManager::standardIcon(QStyle::StandardPixmap pixmap) {
    auto it = standardIcons.constFind(pixmap);
    if (it == standardIcons.constEnd()) it = standardIcons.insert(pixmap, QApplication::style()->standardIcon(pixmap));
    return *it;
}
