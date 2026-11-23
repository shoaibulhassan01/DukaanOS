#include "product_category.h"

#include "../common/record_codec.h"

#include <QFile>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QTextStream>

namespace Pos
{
namespace
{
bool validCategory(const ProductCategory &category)
{
	static const QRegularExpression idPattern(QStringLiteral("^\\d{2}$"));
	const auto validText = [](const QString &value) {
		return !value.trimmed().isEmpty()
			&& !value.contains(QLatin1Char(' '))
			&& !value.contains(QLatin1Char('\t'))
			&& !value.contains(QLatin1Char('#'));
	};
	return idPattern.match(category.id).hasMatch()
		&& validText(category.name)
		&& validText(category.acronym);
}
}

bool readProductCategories(const QString &filePath,
	QVector<ProductCategory> &categories, QString &error)
{
	QFile file(filePath);
	if (!file.exists())
	{
		categories.clear();
		error.clear();
		return true;
	}
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		error = QStringLiteral("Cannot open %1: %2").arg(filePath, file.errorString());
		return false;
	}

	QVector<ProductCategory> loaded;
	QSet<QString> ids;
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
			error = QStringLiteral("Invalid PCategory.txt record at line %1.").arg(lineNumber);
			return false;
		}
		ProductCategory category{fields.at(0), fields.at(1), fields.at(2)};
		if (!validCategory(category) || ids.contains(category.id))
		{
			error = QStringLiteral("Invalid or duplicate category at line %1.").arg(lineNumber);
			return false;
		}
		ids.insert(category.id);
		loaded.append(category);
	}
	categories = loaded;
	error.clear();
	return true;
}

bool saveProductCategories(const QString &filePath,
	const QVector<ProductCategory> &categories, QString &error)
{
	if (categories.isEmpty())
	{
		error = QStringLiteral("Configure at least one product category.");
		return false;
	}
	QStringList records;
	QSet<QString> ids;
	for (int i = 0; i < categories.size(); ++i)
	{
		const ProductCategory &category = categories.at(i);
		if (!validCategory(category) || ids.contains(category.id))
		{
			error = QStringLiteral("Category %1 is invalid or its ID is already used.")
				.arg(category.id.isEmpty() ? QString::number(i + 1) : category.id);
			return false;
		}
		ids.insert(category.id);
		records.append(makeRecord({category.id, category.name, category.acronym}));
	}

	QSaveFile file(filePath);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
	{
		error = QStringLiteral("Cannot write %1: %2").arg(filePath, file.errorString());
		return false;
	}
	QTextStream output(&file);
	for (const QString &record : records)
		output << record << '\n';
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
