
#pragma once

#include <QDate>

namespace Pos
{
struct Season
{
    QDate startDate;
    QDate endDate;
    double percentage = 0;
    bool isActive = false;
};
}
