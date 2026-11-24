
#pragma once

#include <QDate>
#include <QString>

namespace Pos
{
struct Season
{
    QString name;
    QDate startDate;
    QDate endDate;
    double percentage = 0;
    bool isActive = false;
};
}
