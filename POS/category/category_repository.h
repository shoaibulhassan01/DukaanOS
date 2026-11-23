#pragma once

#include "category.h"

#include <QString>
#include <QVector>

namespace Pos
{
// Owner: Umar Khan - reads and writes PCategory.txt.

// A missing file means no categories have been set up yet: returns true with
// an empty list. A line that is not "Id Name Acronym#" is an error.
bool readCategories(QString filePath, QVector<Category> &categories, QString &error);

// Writes every category, one "Id Name Acronym#" line each. The old file is
// only replaced if the whole new file was written.
bool saveCategories(QString filePath, QVector<Category> categories, QString &error);
}
