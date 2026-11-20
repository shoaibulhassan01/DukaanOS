#pragma once

#include <QColor>
#include <QString>

class QApplication;
class QPushButton;
class QWidget;

namespace UiTheme
{
// Owner: Umar Khan - light / dark mode.
//
// The screens are written with light colours. In dark mode every style sheet
// is translated on the fly: text colours become light text colours, card and
// page backgrounds become dark surfaces, status colours (red, green, amber)
// get dark-friendly versions. The bill PDF is not affected (it is for print).
//
// The app starts in the Windows mode (Settings > Personalisation > Colours >
// Choose your mode) and follows it live, until the user clicks the sun / moon
// button; from then on the user's choice is kept while the app is open.
// Either way the app draws its own colours, so light mode looks the same on a
// dark PC and dark mode looks the same on a light PC.
// DUKAANOS_THEME=light or DUKAANOS_THEME=dark forces a starting mode.

// Call once, right after creating the QApplication.
void install(QApplication &app);

bool isDark();

// The user's choice from the sun / moon button.
void setDark(bool dark);
void toggle();

// Round sun / moon button that switches the mode. Shows a moon in light mode
// (click for dark) and a sun in dark mode. onNavy = white icon, for the navy
// header; otherwise the icon uses the text colour.
QPushButton *createToggleButton(QWidget *parent, bool onNavy);

// Style sheet in the current mode (unchanged in light mode).
QString themed(const QString &styleSheet);

// A colour used for text / for a background, in the current mode.
QColor textColor(const QColor &light);
QColor surfaceColor(const QColor &light);
}
