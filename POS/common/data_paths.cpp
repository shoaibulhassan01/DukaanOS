#include "data_paths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

namespace Pos
{
namespace DataPaths
{
QString dataDir()
{
    const QString inWorkingFolder = QDir::current().filePath(QStringLiteral("Data"));
    if (QFileInfo(inWorkingFolder).isDir() || !QCoreApplication::instance())
        return QStringLiteral("Data");

    const QDir applicationDir(QCoreApplication::applicationDirPath());
    const QStringList candidates = {
        QDir::cleanPath(applicationDir.filePath(QStringLiteral("Data"))),
        QDir::cleanPath(applicationDir.filePath(QStringLiteral("../Data"))),
        QDir::cleanPath(applicationDir.filePath(QStringLiteral("../../Data")))
    };
    for (const QString &candidate : candidates)
    {
        if (QFileInfo(candidate).isDir())
            return candidate;
    }
    return QStringLiteral("Data");
}

static QString inData(const QString &name)
{
    return dataDir() + QLatin1Char('/') + name;
}

QString users()         { return inData(QStringLiteral("Users.txt")); }
QString products()      { return inData(QStringLiteral("Product.txt")); }
QString orders()        { return inData(QStringLiteral("Order.txt")); }
QString orderDetail()   { return inData(QStringLiteral("OrderDetail.txt")); }
QString bills()         { return inData(QStringLiteral("Bill.txt")); }
QString barcodeFolder() { return inData(QStringLiteral("barcodes")); }
}
}
