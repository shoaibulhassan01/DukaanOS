#pragma once

#include <QString>
#include <QVector>

namespace Pos
{
struct ProductCategory
{
	QString id;
	QString name;
	QString acronym;
};

bool readProductCategories(const QString &filePath,
	QVector<ProductCategory> &categories, QString &error);
bool saveProductCategories(const QString &filePath,
	const QVector<ProductCategory> &categories, QString &error);
}
