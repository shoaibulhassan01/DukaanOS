#pragma once

#include <QString>
#include <QStringList>

namespace Pos
{
// Shared by Shoaib, Umar, Zain, Anas and Ahad - split a '#' terminated record into fields.
bool splitRecord(QString line, QStringList &fields);

// Shared by Shoaib, Umar, Zain, Anas and Ahad - join fields and append '#'.
QString makeRecord(QStringList fields);
}
