#include "category_repository.h"

#include "../common/record_codec.h"

#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTextStream>

namespace Pos
{
bool readCategories(QString filePath, QVector<Category> &categories, QString &error)
{
    categories.clear();
    if (!QFileInfo::exists(filePath))
    {
        error.clear();
        return true;   // no categories yet
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        error = QStringLiteral("Cannot open %1: %2").arg(filePath, file.errorString());
        return false;
    }

    QVector<Category> loaded;
    QTextStream input(&file);
    int lineNumber = 0;
    while (!input.atEnd())
    {
        const QString line = input.readLine().trimmed();
        ++lineNumber;
        if (line.isEmpty())
            continue;
        QStringList fields;
        if (!splitRecord(line, fields) || fields.size() != 3)
        {
            error = QStringLiteral("PCategory.txt line %1 must be \"Id Name Acronym#\".")
                .arg(lineNumber);
            return false;
        }
        loaded.append(Category{fields.at(0), fields.at(1), fields.at(2)});
    }

    categories = loaded;
    error.clear();
    return true;
}

bool saveCategories(QString filePath, QVector<Category> categories, QString &error)
{
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        error = QStringLiteral("Cannot write %1: %2").arg(filePath, file.errorString());
        return false;
    }

    QTextStream output(&file);
    for (const Category &category : categories)
        output << makeRecord({category.id, category.name, category.acronym}) << '\n';
    output.flush();

    if (output.status() != QTextStream::Ok || !file.commit())
    {
        error = QStringLiteral("Could not save %1: %2").arg(filePath, file.errorString());
        return false;
    }
    error.clear();
    return true;
}
}
