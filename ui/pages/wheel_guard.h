#pragma once

#include <QAbstractSpinBox>
#include <QEvent>
#include <QObject>

namespace UiPages
{
// Number boxes change only by typing (or the - / + buttons), never by
// scrolling the mouse wheel or touchpad over them, which changed amounts
// by accident.
class WheelBlocker : public QObject
{
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::Wheel)
            return true;
        return QObject::eventFilter(watched, event);
    }
};

inline void ignoreMouseWheel(QAbstractSpinBox *box)
{
    box->installEventFilter(new WheelBlocker(box));
}
}
