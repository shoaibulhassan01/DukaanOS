#pragma once

#include "product.h"

#include <QString>
#include <QVector>

namespace Pos
{
// Owner: Umar Khan - read Product.txt and fill the list.
bool readProducts(QString filePath, QVector<Product> &products, QString &error);

// Owner: Umar Khan - save all products back to Product.txt, one record per
// line in the required 10-field order. The old file is only replaced once
// every line has been written, so a failure never leaves a half-written file.
bool saveProducts(QString filePath, QVector<Product> products, QString &error);
}
