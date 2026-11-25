#include "theme.h"

#include <QApplication>
#include <QEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QGuiApplication>
#include <QHash>
#include <QPalette>
#include <QRegularExpression>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleHints>
#include <QWidget>

namespace
{
bool g_dark = false;

// Light colour -> dark colour, for text.
const QHash<QString, QString> &textMap()
{
    static const QHash<QString, QString> map = {
        // ink and muted text
        { "#20233B", "#E7E9F5" }, { "#0F172A", "#E7E9F5" }, { "#0F1B3D", "#E7E9F5" },
        { "#777B91", "#A0A6BF" }, { "#5B6784", "#A0A6BF" }, { "#55596F", "#A0A6BF" },
        { "#52607A", "#A0A6BF" }, { "#475569", "#A0A6BF" },
        { "#A9ABBD", "#6B7190" }, { "#C9CAD6", "#6B7190" }, { "#CBD5E1", "#6B7190" },
        // brand navy used as text -> light indigo
        { "#111184", "#AEB2FF" }, { "#0D0D6B", "#AEB2FF" }, { "#090957", "#AEB2FF" },
        { "#0B1F5C", "#AEB2FF" },
        // status colours
        { "#B42318", "#FF8A80" }, { "#DC2626", "#FF8A80" },
        { "#18764A", "#5FD49A" }, { "#16845B", "#5FD49A" }, { "#0E9F6E", "#5FD49A" },
        { "#D97706", "#FBBF24" }, { "#B7791F", "#FBBF24" }, { "#F59E0B", "#FBBF24" },
        { "#1556E8", "#8AB0FF" }, { "#2563EB", "#8AB0FF" }, { "#1D4ED8", "#8AB0FF" },
        { "#1E40AF", "#8AB0FF" },
        { "#4F46E5", "#ADA8FF" }, { "#5B4BFF", "#ADA8FF" }, { "#7C4DFF", "#ADA8FF" },
        { "#0EA5E9", "#4CC3F5" }, { "#F97316", "#FB923C" },
    };
    return map;
}

// Light colour -> dark colour, for backgrounds and borders.
const QHash<QString, QString> &surfaceMap()
{
    static const QHash<QString, QString> map = {
        // cards
        { "WHITE", "#1A1D30" }, { "#FFFFFF", "#1A1D30" },
        // page backgrounds
        { "#F6F7FB", "#10121F" }, { "#F5F7FB", "#10121F" }, { "#F4F5FB", "#10121F" },
        { "#F2F3FA", "#10121F" }, { "#F8FAFC", "#10121F" }, { "#F0F1F6", "#10121F" },
        // inputs, table headers, hover
        { "#FAFAFD", "#222639" }, { "#FAFAFF", "#222639" }, { "#F8F8FC", "#222639" },
        { "#F6F6FA", "#222639" }, { "#F1F2F7", "#222639" }, { "#E4E5EE", "#2A2E45" },
        // lines and borders
        { "#E7E9F3", "#2D3250" }, { "#DCE3F1", "#2D3250" }, { "#CFD9F2", "#2D3250" },
        { "#CBD5E1", "#2D3250" }, { "#C9CAD6", "#3A3F5A" }, { "#A9ABBD", "#3A3F5A" },
        { "#E1E2FA", "#3A3F7A" },
        // indigo tints (selected rows, light buttons)
        { "#F0F0FF", "#262A55" }, { "#EEF0FF", "#262A55" }, { "#ECEEFF", "#262A55" },
        { "#EEEFFF", "#262A55" }, { "#E8EEFF", "#262A55" }, { "#F0EAFF", "#262A55" },
        { "#E8E9FF", "#30356A" },
        // brand navy as a fill (buttons)
        { "#111184", "#3434C4" }, { "#0D0D6B", "#2A2AA8" }, { "#090957", "#23238F" },
        // status tints
        { "#FEF3F2", "#2E1A1E" }, { "#FFF4F2", "#2E1A1E" }, { "#FEE4E2", "#3A1E23" },
        { "#FECDCA", "#6B2C33" }, { "#F2D5D2", "#6B2C33" },
        { "#F0FBF6", "#12291E" }, { "#E8F8F0", "#12291E" }, { "#E1F7EE", "#12291E" },
        { "#F0FDF4", "#12291E" },
        { "#D6F1E3", "#1E4A33" },
        { "#FEF3DC", "#33270F" }, { "#FFEDE0", "#33270F" }, { "#E0F4FE", "#0F2B3D" },
    };
    return map;
}

// Large navy areas (header, side bar, banner) stay deep instead of bright.
const QHash<QString, QString> &chromeMap()
{
    static const QHash<QString, QString> map = {
        { "#111184", "#15174A" }, { "#0D0D6B", "#15174A" }, { "#090957", "#101238" },
    };
    return map;
}

bool isTextProperty(const QString &property)
{
    return property == QLatin1String("color") || property == QLatin1String("selection-color");
}

QString mapColor(const QString &color, bool text, bool chrome)
{
    const QString key = color.toUpper();
    if (text)
        return textMap().value(key, color);
    if (chrome && chromeMap().contains(key))
        return chromeMap().value(key);
    return surfaceMap().value(key, color);
}

QString translate(const QString &css, bool chrome)
{
    static const QRegularExpression declaration(QStringLiteral("([A-Za-z-]+)\\s*:\\s*([^;{}]+)"));
    static const QRegularExpression colour(
        QStringLiteral("#[0-9A-Fa-f]{6}\\b|\\bwhite\\b"), QRegularExpression::CaseInsensitiveOption);

    QString out;
    out.reserve(css.size());
    int last = 0;
    auto it = declaration.globalMatch(css);
    while (it.hasNext())
    {
        const QRegularExpressionMatch m = it.next();
        const bool text = isTextProperty(m.captured(1).toLower());
        QString value = m.captured(2);
        QString mapped;
        int pos = 0;
        auto colours = colour.globalMatch(value);
        while (colours.hasNext())
        {
            const QRegularExpressionMatch c = colours.next();
            mapped += value.mid(pos, c.capturedStart() - pos);
            // White text on a filled button stays white.
            mapped += (text && c.captured().compare(QLatin1String("white"), Qt::CaseInsensitive) == 0)
                ? c.captured() : mapColor(c.captured(), text, chrome);
            pos = c.capturedEnd();
        }
        mapped += value.mid(pos);
        out += css.mid(last, m.capturedStart(2) - last) + mapped;
        last = m.capturedEnd(2);
    }
    out += css.mid(last);
    return out;
}

bool isChrome(const QWidget *widget)
{
    const QString name = widget->objectName();
    return name == QLatin1String("topHeader") || name == QLatin1String("sideBar") ||
        name == QLatin1String("banner");
}

const char *kOriginal = "_dukaanosLightStyle";
const char *kApplied = "_dukaanosThemedStyle";

// Re-themes a widget's style sheet whenever the app sets a new one.
void applyTo(QWidget *widget)
{
    const QString css = widget->styleSheet();
    if (css.isEmpty() || css == widget->property(kApplied).toString())
        return;   // nothing to do, or this is the sheet we set ourselves
    widget->setProperty(kOriginal, css);
    const QString out = g_dark ? translate(css, isChrome(widget)) : css;
    widget->setProperty(kApplied, out);
    if (out != css)
        widget->setStyleSheet(out);
}

class ThemeFilter : public QObject
{
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if ((event->type() == QEvent::Polish || event->type() == QEvent::StyleChange) &&
            watched->isWidgetType())
            applyTo(static_cast<QWidget *>(watched));
        return QObject::eventFilter(watched, event);
    }
};

bool startingModeIsDark()
{
    const QString forced = qEnvironmentVariable("DUKAANOS_THEME").toLower();
    if (forced == QLatin1String("dark"))
        return true;
    if (forced == QLatin1String("light"))
        return false;
    return false;
}

QPalette makePalette(bool dark)
{
    // The app's own colours, used whatever Windows is set to.
    const QColor window(dark ? "#10121F" : "#F4F5FB");
    const QColor base(dark ? "#1A1D30" : "#FFFFFF");
    const QColor alt(dark ? "#222639" : "#F6F6FA");
    const QColor text(dark ? "#E7E9F5" : "#20233B");
    const QColor muted(dark ? "#6B7190" : "#A9ABBD");
    const QColor accent(dark ? "#4F46E5" : "#111184");
    QPalette p;
    p.setColor(QPalette::Window, window);
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Base, base);
    p.setColor(QPalette::AlternateBase, alt);
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::Button, dark ? alt : base);
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::BrightText, Qt::white);
    p.setColor(QPalette::Highlight, accent);
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::ToolTipBase, QColor("#20233B"));
    p.setColor(QPalette::ToolTipText, Qt::white);
    p.setColor(QPalette::PlaceholderText, dark ? QColor("#6B7190") : QColor("#777B91"));
    p.setColor(QPalette::Link, dark ? QColor("#8AB0FF") : QColor("#1556E8"));
    p.setColor(QPalette::Light, dark ? QColor("#2D3250") : QColor("#FFFFFF"));
    p.setColor(QPalette::Midlight, dark ? QColor("#2A2E45") : QColor("#F1F2F7"));
    p.setColor(QPalette::Mid, dark ? QColor("#2D3250") : QColor("#E7E9F3"));
    p.setColor(QPalette::Dark, dark ? QColor("#0B0D17") : QColor("#C9CAD6"));
    p.setColor(QPalette::Shadow, dark ? QColor("#05060C") : QColor("#A9ABBD"));
    for (QPalette::ColorRole role : { QPalette::WindowText, QPalette::Text, QPalette::ButtonText })
        p.setColor(QPalette::Disabled, role, muted);
    return p;
}

// Sun (switch to light) or moon (switch to dark), drawn at 2x for sharpness.
QPixmap toggleIcon(bool sun, const QColor &color, int size)
{
    const qreal dpr = 2.0;
    QPixmap pm(int(size * dpr), int(size * dpr));
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    const QPointF c(size / 2.0, size / 2.0);
    if (sun)
    {
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawEllipse(c, size * 0.2, size * 0.2);
        p.setPen(QPen(color, size * 0.09, Qt::SolidLine, Qt::RoundCap));
        for (int i = 0; i < 8; ++i)
        {
            p.save();
            p.translate(c);
            p.rotate(i * 45);
            p.drawLine(QPointF(0, -size * 0.32), QPointF(0, -size * 0.43));
            p.restore();
        }
    }
    else
    {
        QPainterPath moon;
        moon.addEllipse(c, size * 0.36, size * 0.36);
        QPainterPath bite;
        bite.addEllipse(QPointF(c.x() + size * 0.2, c.y() - size * 0.14), size * 0.3, size * 0.3);
        p.fillPath(moon.subtracted(bite), color);
    }
    return pm;
}

class ThemeToggleButton : public QPushButton
{
public:
    ThemeToggleButton(QWidget *parent, bool onNavy) : QPushButton(parent), onNavy_(onNavy)
    {
        setCursor(Qt::PointingHandCursor);
        setFixedSize(40, 40);
        setIconSize(QSize(20, 20));
        setStyleSheet(onNavy
            ? QStringLiteral("QPushButton { background: transparent; border: 1px solid rgba(255,255,255,90);"
                             " border-radius: 20px; }"
                             "QPushButton:hover { background: rgba(255,255,255,36); }")
            : QStringLiteral("QPushButton { background: white; border: 1px solid #E7E9F3;"
                             " border-radius: 20px; }"
                             "QPushButton:hover { background: #F0F0FF; }"));
        connect(this, &QPushButton::clicked, this, []() { UiTheme::toggle(); });
        refresh();
    }

    // Called by applyMode whenever the mode changes.
    void refresh()
    {
        const QColor color = onNavy_ ? QColor(Qt::white)
                                     : (g_dark ? QColor("#E7E9F5") : QColor("#20233B"));
        setIcon(QIcon(toggleIcon(g_dark, color, 20)));
        setToolTip(g_dark ? QStringLiteral("Switch to light mode") : QStringLiteral("Switch to dark mode"));
    }

private:
    bool onNavy_;
};

void applyMode(QApplication &app, bool dark)
{
    g_dark = dark;
    // Fusion draws with the app palette only, so nothing picks up the Windows
    // light / dark colours (that is what gave the beige background).
    if (!app.style() || app.style()->name().compare(QLatin1String("fusion"), Qt::CaseInsensitive) != 0)
        app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    app.setPalette(makePalette(dark));
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    // Window title bar and native dialogs in the same mode as the app.
    QGuiApplication::styleHints()->setColorScheme(dark ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light);
#endif

    // Re-theme everything that already exists.
    for (QWidget *widget : QApplication::allWidgets())
    {
        const QString original = widget->property(kOriginal).toString();
        if (!original.isEmpty())
        {
            const QString out = dark ? translate(original, isChrome(widget)) : original;
            widget->setProperty(kApplied, out);
            widget->setStyleSheet(out);
        }
        if (auto *toggle = dynamic_cast<ThemeToggleButton *>(widget))
            toggle->refresh();
        widget->update();
    }
}

}

void UiTheme::install(QApplication &app)
{
    app.installEventFilter(new ThemeFilter(&app));
    applyMode(app, startingModeIsDark());
}

void UiTheme::setDark(bool dark)
{
    if (auto *app = qobject_cast<QApplication *>(QCoreApplication::instance()))
        applyMode(*app, dark);
}

void UiTheme::toggle()
{
    setDark(!g_dark);
}

QPushButton *UiTheme::createToggleButton(QWidget *parent, bool onNavy)
{
    return new ThemeToggleButton(parent, onNavy);
}

bool UiTheme::isDark()
{
    return g_dark;
}

QString UiTheme::themed(const QString &styleSheet)
{
    return g_dark ? translate(styleSheet, false) : styleSheet;
}

QColor UiTheme::textColor(const QColor &light)
{
    return g_dark ? QColor(mapColor(light.name(QColor::HexRgb), true, false)) : light;
}

QColor UiTheme::surfaceColor(const QColor &light)
{
    return g_dark ? QColor(mapColor(light.name(QColor::HexRgb), false, false)) : light;
}
