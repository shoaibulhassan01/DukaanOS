#pragma once

#include <QCoreApplication>
#include <QEvent>
#include <QObject>
#include <QWidget>

namespace UiPages
{
// Number boxes and drop-downs (e.g. the payment method) change only by
// typing or clicking, never by scrolling the mouse wheel or touchpad over
// them, which changed amounts and Cash / Card / Wallet by accident. The
// scroll is passed on, so the page itself still scrolls.
class WheelBlocker : public QObject
{
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::Wheel)
        {
            if (auto *widget = qobject_cast<QWidget *>(watched))
                if (QWidget *parent = widget->parentWidget())
                    QCoreApplication::sendEvent(parent, event);
            return true;
        }
        return QObject::eventFilter(watched, event);
    }
};

inline void ignoreMouseWheel(QWidget *widget)
{
    widget->installEventFilter(new WheelBlocker(widget));
}
}
