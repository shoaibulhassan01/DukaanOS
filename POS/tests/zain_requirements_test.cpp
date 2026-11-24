#include "../bill/bill_repository.h"
#include "../common/data_paths.h"
#include "../order/order_repository.h"
#include "../order/order_service.h"
#include "../product/barcode_image.h"
#include "../product/product_category.h"
#include "../product/product_repository.h"
#include "../product/product_service.h"
#include "../../BarCode/decoder/decoder.h"
#include "../../BarCode/scanner/scanner.h"

#include <QDate>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QTemporaryDir>
#include <QTextStream>

namespace
{
int failures = 0;

void check(bool condition, const QString &message)
{
	if (!condition)
	{
		QTextStream(stderr) << "FAIL: " << message << Qt::endl;
		++failures;
	}
}

bool writeFile(const QString &path, const QByteArray &contents)
{
	QFile file(path);
	return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
		&& file.write(contents) == contents.size();
}

void testCategoryBarcode(const QString &root)
{
	const QString dataDir = QDir(root).filePath(QStringLiteral("Data"));
	QDir().mkpath(dataDir);
	const QString productFile = QDir(dataDir).filePath(QStringLiteral("Product.txt"));
	const QString categoryFile = QDir(dataDir).filePath(QStringLiteral("PCategory.txt"));
	QString error;

	check(Pos::saveProductCategories(categoryFile,
		{{QStringLiteral("10"), QStringLiteral("Drinks"), QStringLiteral("DRK")}}, error),
		QStringLiteral("Could not save product category: %1").arg(error));

	QString barcode;
	check(Pos::generateNextProductBarcode(productFile, QStringLiteral("10"), barcode, error)
			&& barcode == QStringLiteral("100001"),
		QStringLiteral("Category 10 should generate barcode 100001: %1").arg(error));
	check(!Pos::generateNextProductBarcode(productFile, QStringLiteral("99"), barcode, error),
		QStringLiteral("An unconfigured category generated a barcode."));

	Pos::Product product;
	product.id = QStringLiteral("P1001");
	product.categoryId = QStringLiteral("10");
	product.acronym = QStringLiteral("TEA");
	product.name = QStringLiteral("Tea");
	product.unit = QStringLiteral("count");
	product.unitPrice = 20;
	product.tax = 1;
	product.discountPerUnit = 2;
	product.quantity = 8;
	check(Pos::addProduct(productFile, product, error),
		QStringLiteral("Could not add category product: %1").arg(error));

	QVector<Pos::Product> saved;
	check(Pos::readProducts(productFile, saved, error) && saved.size() == 1,
		QStringLiteral("Generated product was not saved: %1").arg(error));
	if (saved.size() != 1)
		return;
	check(saved.first().barcode == QStringLiteral("100001")
			&& saved.first().categoryId == QStringLiteral("10"),
		QStringLiteral("Product barcode/category was saved incorrectly."));

	const QString imagePath = QDir(dataDir).filePath(QStringLiteral("barcodes/tea_100001.png"));
	const QImage image(imagePath);
	check(!image.isNull(), QStringLiteral("Generated barcode PNG cannot be opened."));
	if (image.isNull())
		return;
	const std::string pattern = imageToString(image);
	std::string decoded;
	std::string decodeError;
	check(!pattern.empty() && decodeRuns(pattern, decoded, decodeError)
			&& decoded == "100001",
		QStringLiteral("Saved barcode did not decode to 100001: %1")
			.arg(QString::fromStdString(decodeError)));
}

void testReportsAndOrderEditing(const QString &root)
{
	QDir().mkpath(QDir(root).filePath(QStringLiteral("Data")));
	QDir::setCurrent(root);
	const QDate today = QDate::currentDate();
	const QString paidDate = today.addDays(-3).toString(Qt::ISODate);
	const QString oldPaidDate = today.addDays(-8).toString(Qt::ISODate);
	const QString billId = today.addDays(-3).toString(QStringLiteral("yyMMdd"))
		+ QStringLiteral("001");
	QString error;

	check(writeFile(QStringLiteral("Data/Product.txt"),
		QByteArrayLiteral("P1 100001 TEA Tea count 20.00 1.00 2.00 8 Data/barcodes/100001.png#\n")),
		QStringLiteral("Could not create test Product.txt."));
	check(writeFile(QStringLiteral("Data/Order.txt"),
		QStringLiteral("00000001 %1 10:00:00 Paid_customer - paid 38.00 cash -#\n"
					   "00000002 %1 10:30:00 Held_customer - on-hold 0.00 cash -#\n"
					   "00000003 %1 11:00:00 Archived_customer - archived 12.00 cash -#\n"
					   "00000004 %1 11:30:00 Active_customer - in-progress 20.00 cash -#\n")
			.arg(today.toString(Qt::ISODate)).toUtf8()),
		QStringLiteral("Could not create test Order.txt."));
	check(writeFile(QStringLiteral("Data/OrderDetail.txt"),
		QByteArrayLiteral("00000001 P1 100001 2#\n00000002 P1 100001 2#\n"
			"00000003 P1 100001 1#\n00000004 P1 100001 1#\n")),
		QStringLiteral("Could not create test OrderDetail.txt."));
	check(writeFile(QStringLiteral("Data/Bill.txt"),
		QStringLiteral("%1 Paid_customer 00000001 40.00 2.00 4.00 38.00 "
					   "38.00 0.00 paid 0.00 0.00 %2 10:00:00#\n")
			.arg(billId, paidDate).toUtf8()),
		QStringLiteral("Could not create test Bill.txt."));
	check(writeFile(QStringLiteral("Data/BillDetail.txt"),
		QStringLiteral("%1 TEA 20.00 2 4.00#\n").arg(billId).toUtf8()),
		QStringLiteral("Could not create test BillDetail.txt."));

	Pos::OrderReport incomplete;
	check(Pos::readIncompleteOrdersReport(incomplete, error)
			&& incomplete.orders.size() == 2 && incomplete.totalAmount == 20.00,
		QStringLiteral("Incomplete report had wrong orders/total: %1").arg(error));
	Pos::OrderReport completed;
	check(Pos::readCompletedOrdersReport(completed, error)
			&& completed.orders.size() == 2 && completed.totalAmount == 50.00,
		QStringLiteral("Completed report had wrong orders/total: %1").arg(error));

	Pos::OrderSession paid;
	bool managerApprovalRequired = false;
	check(!Pos::openOrderForEditing(QStringLiteral("00000001"), false, paid, error,
			&managerApprovalRequired)
			&& managerApprovalRequired
			&& error.contains(QStringLiteral("approval"), Qt::CaseInsensitive),
		QStringLiteral("Paid order did not require manager approval."));
	check(Pos::openOrderForEditing(QStringLiteral("00000001"), true, paid, error),
		QStringLiteral("Approved 3-day paid order would not open: %1").arg(error));
	if (!paid.items.isEmpty())
	{
		paid.items[0].quantity = 11;
		check(!Pos::saveOrderEdits(paid, true, error),
			QStringLiteral("Over-stock edit was accepted."));
		paid.items[0].quantity = 3;
		check(Pos::saveOrderEdits(paid, true, error),
			QStringLiteral("Approved paid edit did not save: %1").arg(error));
	}

	QVector<Pos::Product> products;
	check(Pos::readProducts(QStringLiteral("Data/Product.txt"), products, error)
			&& products.size() == 1 && products.first().quantity == 7,
		QStringLiteral("Paid edit did not update inventory correctly: %1").arg(error));
	QVector<Pos::OrderDetail> details;
	check(Pos::readOrderDetails(QStringLiteral("Data/OrderDetail.txt"),
			QStringLiteral("00000001"), details, error)
			&& details.size() == 1 && details.first().quantity == 3,
		QStringLiteral("Paid edit did not save OrderDetail.txt: %1").arg(error));

	check(writeFile(QStringLiteral("Data/Bill.txt"),
		QStringLiteral("%1 Paid_customer 00000001 40.00 2.00 4.00 38.00 "
					   "38.00 0.00 paid 0.00 0.00 %2 10:00:00#\n")
			.arg(billId, oldPaidDate).toUtf8()),
		QStringLiteral("Could not set test paid date."));
	Pos::OrderSession expired;
	managerApprovalRequired = true;
	check(!Pos::openOrderForEditing(QStringLiteral("00000001"), false, expired, error,
			&managerApprovalRequired)
			&& !managerApprovalRequired && error.contains(QStringLiteral("7 days")),
		QStringLiteral("Paid order older than seven days was not refused."));

	Pos::OrderSession held;
	managerApprovalRequired = true;
	check(Pos::openOrderForEditing(QStringLiteral("00000002"), false, held, error,
			&managerApprovalRequired) && !managerApprovalRequired,
		QStringLiteral("On-hold order should open without approval: %1").arg(error));
	if (!held.items.isEmpty())
		held.items[0].quantity = 4;
	check(Pos::saveOrderEdits(held, false, error),
		QStringLiteral("On-hold order did not save without approval: %1").arg(error));
	check(Pos::readProducts(QStringLiteral("Data/Product.txt"), products, error)
			&& products.size() == 1 && products.first().quantity == 7,
		QStringLiteral("Editing on-hold order changed stock."));
	check(Pos::readOrderDetails(QStringLiteral("Data/OrderDetail.txt"),
			QStringLiteral("00000002"), details, error)
			&& details.size() == 1 && details.first().quantity == 4,
		QStringLiteral("On-hold edit did not save OrderDetail.txt: %1").arg(error));
}
}

int main(int argc, char *argv[])
{
	qputenv("QT_QPA_PLATFORM", "offscreen");
	QGuiApplication app(argc, argv);
	QTemporaryDir temporary;
	if (!temporary.isValid())
	{
		QTextStream(stderr) << "Could not create a temporary test folder." << Qt::endl;
		return 1;
	}

	const QString originalDirectory = QDir::currentPath();
	testCategoryBarcode(temporary.filePath(QStringLiteral("barcode")));
	testReportsAndOrderEditing(temporary.filePath(QStringLiteral("orders")));
	QDir::setCurrent(originalDirectory);

	if (failures != 0)
	{
		QTextStream(stderr) << failures << " test(s) failed." << Qt::endl;
		return 1;
	}
	QTextStream(stdout) << "Zain requirements test passed." << Qt::endl;
	return 0;
}
