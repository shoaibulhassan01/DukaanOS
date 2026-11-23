#pragma once

#include "product.h"

#include <QString>
#include <QVector>

namespace Pos
{
// Owner: Umar Khan - read Product.txt and fill the list.
bool readProducts(QString filePath, QVector<Product> &products, QString &error);

// Owner: Umar Khan - save all products back to Product.txt, one record per
// line. Legacy 10-field records are still read; writes include the Category Id
// as the final field. The old file is replaced only after a complete write.
bool saveProducts(QString filePath, QVector<Product> products, QString &error);
}
