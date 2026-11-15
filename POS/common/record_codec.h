#pragma once

#include <QString>
#include <QStringList>

namespace Pos
{
// Shared work, split evenly across the team. TODO: Split one record such as "P1 123 APL#".
bool splitRecord(QString line, QStringList &fields);

// Shared work, split evenly across the team. TODO: Join fields and add the ending #.
QString makeRecord(QStringList fields);
}
