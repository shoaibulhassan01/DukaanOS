// Owner: Umar Khan - see data_format_checker.h.
//
// Where the rules come from (Increment 3 requirements unless noted):
//   every file   one record per line, fields separated by ONE space, the line
//                ends with '#' (req 1, 8, 9, 13 and the earlier increments);
//                no blank lines, tabs or spaces before '#'.
//   PCategory    Id (2 digits, unique: it starts every barcode), Name, Acronym
//                (req 1).
//   Product      the 10 fields of the earlier increments plus the Category Id
//                (req 2). The requirements do not say WHERE the Category Id
//                goes, so it is accepted in any position as long as it is the
//                same on every line and equals the first two digits of the
//                barcode (req 1). Product Id is P + exactly 4 digits
//                (P0001): a team rule, not a requirement, so every member's
//                data uses the same Id style; new Ids come from addProduct.
//   Bill         exactly these 14 fields in this order (req 8): Bill Id (9
//                digits, never repeated), customer name, Order Id (in
//                Order.txt), total without tax, total tax, total discount,
//                total payable, amount received, balance returned, status
//                (= the order's status in Order.txt), seasonal discount,
//                special discount, bill date, bill time.
//                total payable = without tax + tax - discount - seasonal -
//                special; balance returned = received - payable (paid bills).
//                Special discount is at most 8 percent: checked against the
//                bill before any discount, the largest base the requirement
//                could mean. A seasonal discount above 0 needs a season in
//                Seasons.txt whose dates include the bill date (req 15).
//   BillDetail   Bill Id (in Bill.txt), product acronym, unit price without
//                tax, quantity, discount for that line (req 9). The lines of
//                a bill add up to its "total without tax" and "total discount"
//                (req 8: "sum of discounts on all bought products").
//   Seasons      start date, end date, discount percentage, isActive
//                ("true"/"false") (req 13). Start <= end, 0 < percentage <= 100.
//   Order, OrderDetail, Users: the formats of the earlier increments.
// Dates are yyyy-MM-dd and times HH:mm:ss, as in Order.txt ("as before").
// Money has exactly 2 decimals, as in every existing file.

#include "data_format_checker.h"

#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QTime>

#include <algorithm>
#include <cmath>

namespace DataFormat
{
namespace
{
const QStringList kRequiredFiles = {
    QStringLiteral("Users.txt"), QStringLiteral("Product.txt"), QStringLiteral("Order.txt"),
    QStringLiteral("OrderDetail.txt"), QStringLiteral("Bill.txt"), QStringLiteral("BillDetail.txt")};
const QStringList kNewFiles = {QStringLiteral("PCategory.txt"), QStringLiteral("Seasons.txt")};
const QStringList kOrderStatuses = {
    QStringLiteral("paid"), QStringLiteral("on-hold"), QStringLiteral("archived")};

struct Record
{
    int line = 0;
    QStringList f;
};

struct Product
{
    int line = 0;
    QString id, barcode, acronym, categoryId;
};

bool matches(const QString &text, const char *pattern)
{
    return QRegularExpression(QStringLiteral("^%1$").arg(QLatin1String(pattern))).match(text).hasMatch();
}

bool isMoney(const QString &s) { return matches(s, "\\d+\\.\\d{2}"); }
bool isCount(const QString &s) { return matches(s, "\\d+"); }
bool isPositiveCount(const QString &s) { return isCount(s) && s.toLongLong() > 0; }
bool isDate(const QString &s)
{
    return matches(s, "\\d{4}-\\d{2}-\\d{2}") && QDate::fromString(s, Qt::ISODate).isValid();
}
bool isTime(const QString &s)
{
    return matches(s, "\\d{2}:\\d{2}:\\d{2}") && QTime::fromString(s, QStringLiteral("HH:mm:ss")).isValid();
}
bool sameMoney(double a, double b) { return std::abs(a - b) < 0.005; }
QString money(double v) { return QString::number(v, 'f', 2); }

QString dataPath(const QString &projectDir, const QString &file)
{
    return QDir(projectDir).filePath(QStringLiteral("Data/") + file);
}

bool exists(const QString &projectDir, const QString &file)
{
    // Exact name, also on Windows where the file system ignores letter case.
    return QDir(QDir(projectDir).filePath(QStringLiteral("Data")))
        .entryList(QDir::Files).contains(file);
}

// Reads one data file. Problems with the line layout are added to problems;
// lines with the right number of fields are returned for the field checks.
bool readFile(const QString &projectDir, const QString &file, QList<int> fieldCounts,
    QVector<Record> &records, QStringList &problems, const QString &countHint = QString())
{
    records.clear();
    if (!exists(projectDir, file))
        return false;
    QFile in(dataPath(projectDir, file));
    if (!in.open(QIODevice::ReadOnly))
    {
        problems << QStringLiteral("%1 cannot be opened.").arg(file);
        return true;
    }
    QStringList lines = QString::fromUtf8(in.readAll()).split(QLatin1Char('\n'));
    if (!lines.isEmpty() && lines.last().isEmpty())
        lines.removeLast();   // the newline after the last record
    QStringList expected;
    for (int n : fieldCounts)
        expected << QString::number(n);
    for (int i = 0; i < lines.size(); ++i)
    {
        QString line = lines.at(i);
        if (line.endsWith(QLatin1Char('\r')))
            line.chop(1);
        const QString where = QStringLiteral("%1 line %2").arg(file).arg(i + 1);
        if (line.trimmed().isEmpty())
        {
            problems << QStringLiteral("%1 is blank. Remove empty lines.").arg(where);
            continue;
        }
        if (!line.endsWith(QLatin1Char('#')))
        {
            problems << QStringLiteral("%1 does not end with '#': %2").arg(where, line);
        }
        if (line.count(QLatin1Char('#')) > 1)
        {
            problems << QStringLiteral("%1 has more than one '#' (only one, at the end): %2").arg(where, line);
        }
        const QString body = line.endsWith(QLatin1Char('#')) ? line.chopped(1) : line;
        if (body.contains(QLatin1Char('\t')))
        {
            problems << QStringLiteral("%1 uses a tab; fields must be separated by one space.").arg(where);
        }
        if (body.startsWith(QLatin1Char(' ')) || body.endsWith(QLatin1Char(' ')))
        {
            problems << QStringLiteral("%1 has a space at the start or before '#': %2").arg(where, line);
        }
        if (body.contains(QStringLiteral("  ")))
        {
            problems << QStringLiteral("%1 has two spaces in a row: %2").arg(where, line);
        }
        const QStringList fields = body.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (!fieldCounts.contains(fields.size()))
        {
            problems << QStringLiteral("%1 has %2 fields, expected %3%4: %5")
                .arg(where).arg(fields.size()).arg(expected.join(QStringLiteral(" or ")), countHint, line);
            continue;
        }
        // Layout problems are reported above; the fields are still checked.
        records.append(Record{i + 1, fields});
    }
    return true;
}

QString at(const QString &file, const Record &r)
{
    return QStringLiteral("%1 line %2").arg(file).arg(r.line);
}

// ------------------------------------------------------------------ products
// The 10 fields of the earlier increments: id barcode acronym name unit price
// tax discount quantity image.
QStringList productProblems(const QStringList &f, const QString &projectDir)
{
    QStringList p;
    if (!matches(f.at(1), "\\d{6}"))
        p << QStringLiteral("barcode '%1' must be 6 digits").arg(f.at(1));
    if (f.at(4) != QStringLiteral("count") && f.at(4) != QStringLiteral("weight"))
        p << QStringLiteral("unit '%1' must be count or weight").arg(f.at(4));
    if (!isMoney(f.at(5)) || f.at(5).toDouble() <= 0)
        p << QStringLiteral("unit price '%1' must be above 0 with 2 decimals").arg(f.at(5));
    if (!isMoney(f.at(6)))
        p << QStringLiteral("tax '%1' must be money with 2 decimals").arg(f.at(6));
    if (!isMoney(f.at(7)))
        p << QStringLiteral("discount '%1' must be money with 2 decimals").arg(f.at(7));
    else if (isMoney(f.at(5)) && f.at(7).toDouble() > f.at(5).toDouble())
        p << QStringLiteral("discount %1 is more than the price %2").arg(f.at(7), f.at(5));
    if (!isCount(f.at(8)))
        p << QStringLiteral("quantity '%1' must be a whole number, 0 or more").arg(f.at(8));
    if (!QFileInfo::exists(QDir(projectDir).filePath(f.at(9))))
        p << QStringLiteral("barcode image '%1' does not exist").arg(f.at(9));
    return p;
}

struct ProductFile
{
    bool found = false;
    QVector<Record> records;
    int withoutCategory = 0;     // lines with the 10 old fields
    int withCategory = 0;        // lines with 11 fields
    int categoryPosition = -1;   // field number of the Category Id, -1 if unknown
    QStringList categoryProblems;
    QVector<Product> products;   // lines whose fields could be read
};

ProductFile loadProducts(const QString &projectDir, QStringList &problems)
{
    ProductFile pf;
    pf.found = readFile(projectDir, QStringLiteral("Product.txt"), {10, 11}, pf.records, problems,
        QStringLiteral(" (10 fields, plus the Category Id = 11)"));
    if (!pf.found)
        return pf;

    // Where is the Category Id? Any position that, on EVERY 11-field line,
    // holds 2 digits equal to the start of the barcode, with the remaining 10
    // fields in the old order.
    QSet<int> common;
    bool first = true;
    for (const Record &r : pf.records)
    {
        if (r.f.size() == 10)
        {
            ++pf.withoutCategory;
            continue;
        }
        ++pf.withCategory;
        QSet<int> here;
        for (int pos = 0; pos < 11; ++pos)
        {
            QStringList rest = r.f;
            const QString category = rest.takeAt(pos);
            if (matches(category, "\\d{2}") && matches(rest.at(1), "\\d{6}") &&
                rest.at(1).startsWith(category) && isMoney(rest.at(5)) && isCount(rest.at(8)))
                here.insert(pos);
        }
        if (here.isEmpty())
            pf.categoryProblems << QStringLiteral(
                "%1 has no 2-digit Category Id equal to the first two digits of its barcode (req 1, 2): %2")
                .arg(at(QStringLiteral("Product.txt"), r), r.f.join(QLatin1Char(' ')));
        common = first ? here : (common & here);
        first = false;
    }
    if (pf.withCategory > 0 && pf.withoutCategory > 0)
        pf.categoryProblems << QStringLiteral(
            "Product.txt: %1 line(s) have a Category Id and %2 do not; every product needs one (req 2).")
            .arg(pf.withCategory).arg(pf.withoutCategory);
    if (pf.withCategory > 0 && pf.categoryProblems.isEmpty())
    {
        if (common.isEmpty())
            pf.categoryProblems << QStringLiteral(
                "Product.txt: the Category Id is not in the same field on every line.");
        else
            pf.categoryPosition = *std::min_element(common.begin(), common.end());
    }

    for (const Record &r : pf.records)
    {
        QStringList base = r.f;
        Product product;
        product.line = r.line;
        if (r.f.size() == 11)
        {
            if (pf.categoryPosition < 0)
                continue;   // reported by the category check
            product.categoryId = base.takeAt(pf.categoryPosition);
        }
        product.id = base.at(0);
        product.barcode = base.at(1);
        product.acronym = base.at(2);
        for (const QString &problem : productProblems(base, projectDir))
            problems << QStringLiteral("%1: %2.").arg(at(QStringLiteral("Product.txt"), r), problem);
        pf.products.append(product);
    }
    return pf;
}

// -------------------------------------------------------------------- groups
Result finish(Report &report)
{
    return report.problems.isEmpty() ? Result::Passed : Result::Failed;
}

Result skip(Report &report, const QString &reason)
{
    report.skipReason = reason;
    return Result::Skipped;
}

Result checkFileNames(const QString &projectDir, Report &report)
{
    const QDir data(QDir(projectDir).filePath(QStringLiteral("Data")));
    if (!data.exists())
    {
        report.problems << QStringLiteral("The Data folder is missing.");
        return Result::Failed;
    }
    const QStringList present = data.entryList({QStringLiteral("*.txt"), QStringLiteral("*.TXT")}, QDir::Files);
    const QStringList known = kRequiredFiles + kNewFiles;
    auto sameIgnoringCase = [&](const QString &name) {
        for (const QString &p : present)
            if (p.compare(name, Qt::CaseInsensitive) == 0)
                return p;
        return QString();
    };
    for (const QString &name : known)
    {
        const QString found = sameIgnoringCase(name);
        if (!found.isEmpty() && found != name)
            report.problems << QStringLiteral("Data/%1 must be named exactly %2 (letter case).").arg(found, name);
        else if (found.isEmpty() && kRequiredFiles.contains(name))
            report.problems << QStringLiteral("Data/%1 is missing.").arg(name);
    }
    for (const QString &p : present)
    {
        bool isKnown = false;
        for (const QString &name : known)
            isKnown = isKnown || p.compare(name, Qt::CaseInsensitive) == 0;
        if (isKnown)
            continue;
        QString hint;
        const QString stem = QFileInfo(p).completeBaseName().toLower();
        for (const QString &name : known)
        {
            const QString knownStem = QFileInfo(name).completeBaseName().toLower();
            if (stem.contains(knownStem.left(5)) || knownStem.contains(stem.left(5)))
                hint = QStringLiteral(" Did you mean %1?").arg(name);
        }
        report.problems << QStringLiteral("Data/%1 is not one of the data files in the requirements (%2).%3")
            .arg(p, known.join(QStringLiteral(", ")), hint);
    }
    if (!QFileInfo(data.filePath(QStringLiteral("barcodes"))).isDir())
        report.problems << QStringLiteral("Data/barcodes (the barcode images) is missing.");
    return finish(report);
}

Result checkUsers(const QString &projectDir, Report &report)
{
    QVector<Record> users;
    if (!readFile(projectDir, QStringLiteral("Users.txt"), {3}, users, report.problems,
            QStringLiteral(" (username password role)")))
    {
        report.problems << QStringLiteral("Data/Users.txt is missing.");
        return Result::Failed;
    }
    QSet<QString> names;
    int managers = 0;
    int cashiers = 0;
    for (const Record &r : users)
    {
        if (names.contains(r.f.at(0)))
            report.problems << QStringLiteral("%1: username %2 is used twice.").arg(at(QStringLiteral("Users.txt"), r), r.f.at(0));
        names.insert(r.f.at(0));
        if (r.f.at(2) == QStringLiteral("manager"))
            ++managers;
        else if (r.f.at(2) == QStringLiteral("cashier"))
            ++cashiers;
        else
            report.problems << QStringLiteral("%1: role '%2' must be manager or cashier.").arg(at(QStringLiteral("Users.txt"), r), r.f.at(2));
    }
    if (managers == 0 || cashiers == 0)
        report.problems << QStringLiteral("Users.txt needs at least one manager and one cashier (req 12).");
    return finish(report);
}

Result checkProducts(const QString &projectDir, Report &report)
{
    const ProductFile pf = loadProducts(projectDir, report.problems);
    if (!pf.found)
    {
        report.problems << QStringLiteral("Data/Product.txt is missing.");
        return Result::Failed;
    }
    QHash<QString, int> ids;
    QHash<QString, int> barcodes;
    static const QRegularExpression productId(QStringLiteral("^P\\d{4}$"));
    for (const Product &p : pf.products)
    {
        if (!productId.match(p.id).hasMatch())
            report.problems << QStringLiteral("Product.txt line %1: product id %2 must be P followed by 4 digits, e.g. P0016.")
                .arg(p.line).arg(p.id);
        if (ids.contains(p.id))
            report.problems << QStringLiteral("Product.txt line %1: product id %2 is already used on line %3.")
                .arg(p.line).arg(p.id).arg(ids.value(p.id));
        if (barcodes.contains(p.barcode))
            report.problems << QStringLiteral("Product.txt line %1: barcode %2 is already used on line %3.")
                .arg(p.line).arg(p.barcode).arg(barcodes.value(p.barcode));
        ids.insert(p.id, p.line);
        barcodes.insert(p.barcode, p.line);
    }
    return finish(report);
}

QVector<Record> loadCategories(const QString &projectDir, QStringList &problems, bool &found)
{
    QVector<Record> categories;
    found = readFile(projectDir, QStringLiteral("PCategory.txt"), {3}, categories, problems,
        QStringLiteral(" (Category Id, Name, Acronym)"));
    return categories;
}

Result checkPCategory(const QString &projectDir, Report &report)
{
    bool found = false;
    const QVector<Record> categories = loadCategories(projectDir, report.problems, found);
    if (!found)
        return skip(report, QStringLiteral("Data/PCategory.txt does not exist yet (req 1)."));
    QHash<QString, int> ids;
    for (const Record &r : categories)
    {
        const QString where = at(QStringLiteral("PCategory.txt"), r);
        if (!matches(r.f.at(0), "\\d{2}"))
            report.problems << QStringLiteral("%1: Category Id '%2' must be exactly 2 digits.").arg(where, r.f.at(0));
        else if (ids.contains(r.f.at(0)))
            report.problems << QStringLiteral("%1: Category Id %2 is already used on line %3.")
                .arg(where, r.f.at(0)).arg(ids.value(r.f.at(0)));
        ids.insert(r.f.at(0), r.line);
    }
    return finish(report);
}

Result checkProductCategories(const QString &projectDir, Report &report)
{
    QStringList ignored;
    const ProductFile pf = loadProducts(projectDir, ignored);
    bool categoriesFound = false;
    const QVector<Record> categories = loadCategories(projectDir, ignored, categoriesFound);
    if (!categoriesFound && pf.withCategory == 0)
        return skip(report, QStringLiteral(
            "Req 1-2 not started: no PCategory.txt and no Category Id in Product.txt yet."));
    if (!pf.found)
    {
        report.problems << QStringLiteral("Data/Product.txt is missing.");
        return Result::Failed;
    }
    if (!categoriesFound)
        report.problems << QStringLiteral("Product.txt has Category Ids but Data/PCategory.txt is missing (req 1).");
    if (pf.withCategory == 0 && pf.withoutCategory > 0)
        report.problems << QStringLiteral(
            "PCategory.txt exists but Product.txt has no Category Id field: every product needs one (req 2).");
    report.problems << pf.categoryProblems;
    QSet<QString> known;
    for (const Record &r : categories)
        known.insert(r.f.at(0));
    if (categoriesFound)
        for (const Product &p : pf.products)
            if (!p.categoryId.isEmpty() && !known.contains(p.categoryId))
                report.problems << QStringLiteral("Product.txt line %1: Category Id %2 is not in PCategory.txt.")
                    .arg(p.line).arg(p.categoryId);
    return finish(report);
}

QHash<QString, Record> loadOrders(const QString &projectDir, QStringList &problems, bool &found)
{
    QVector<Record> records;
    found = readFile(projectDir, QStringLiteral("Order.txt"), {9}, records, problems,
        QStringLiteral(" (id date time customer contact status amount mode merchant)"));
    QHash<QString, Record> orders;
    for (const Record &r : records)
        orders.insert(r.f.at(0), r);
    return orders;
}

Result checkOrders(const QString &projectDir, Report &report)
{
    QVector<Record> orders;
    if (!readFile(projectDir, QStringLiteral("Order.txt"), {9}, orders, report.problems,
            QStringLiteral(" (id date time customer contact status amount mode merchant)")))
    {
        report.problems << QStringLiteral("Data/Order.txt is missing.");
        return Result::Failed;
    }
    QHash<QString, int> ids;
    for (const Record &r : orders)
    {
        const QString where = at(QStringLiteral("Order.txt"), r);
        const QStringList &f = r.f;
        if (!matches(f.at(0), "\\d{8}"))
            report.problems << QStringLiteral("%1: order id '%2' must be 8 digits.").arg(where, f.at(0));
        else if (ids.contains(f.at(0)))
            report.problems << QStringLiteral("%1: order id %2 is already used on line %3.")
                .arg(where, f.at(0)).arg(ids.value(f.at(0)));
        ids.insert(f.at(0), r.line);
        if (!isDate(f.at(1)))
            report.problems << QStringLiteral("%1: date '%2' must be yyyy-MM-dd.").arg(where, f.at(1));
        if (!isTime(f.at(2)))
            report.problems << QStringLiteral("%1: time '%2' must be HH:mm:ss.").arg(where, f.at(2));
        if (!kOrderStatuses.contains(f.at(5)))
            report.problems << QStringLiteral("%1: status '%2' must be %3.").arg(where, f.at(5), kOrderStatuses.join(QStringLiteral(", ")));
        if (!isMoney(f.at(6)))
            report.problems << QStringLiteral("%1: amount '%2' must be money with 2 decimals.").arg(where, f.at(6));
        if (f.at(7) == QStringLiteral("cash"))
        {
            if (f.at(8) != QStringLiteral("-"))
                report.problems << QStringLiteral("%1: a cash order has no merchant ('-').").arg(where);
        }
        else if (f.at(7) == QStringLiteral("card") || f.at(7) == QStringLiteral("wallet"))
        {
            if (f.at(8) == QStringLiteral("-"))
                report.problems << QStringLiteral("%1: a %2 payment needs a merchant.").arg(where, f.at(7));
        }
        else
            report.problems << QStringLiteral("%1: payment mode '%2' must be cash, card or wallet.").arg(where, f.at(7));
    }

    QVector<Record> details;
    if (!readFile(projectDir, QStringLiteral("OrderDetail.txt"), {4}, details, report.problems,
            QStringLiteral(" (order id, product id, barcode, quantity)")))
    {
        report.problems << QStringLiteral("Data/OrderDetail.txt is missing.");
        return Result::Failed;
    }
    QStringList ignored;
    const ProductFile pf = loadProducts(projectDir, ignored);
    QHash<QString, QString> barcodeOf;
    for (const Product &p : pf.products)
        barcodeOf.insert(p.id, p.barcode);
    QSet<QString> withItems;
    for (const Record &r : details)
    {
        const QString where = at(QStringLiteral("OrderDetail.txt"), r);
        const QStringList &f = r.f;
        withItems.insert(f.at(0));
        if (!ids.contains(f.at(0)))
            report.problems << QStringLiteral("%1: order %2 is not in Order.txt.").arg(where, f.at(0));
        if (!barcodeOf.contains(f.at(1)))
            report.problems << QStringLiteral("%1: product %2 is not in Product.txt.").arg(where, f.at(1));
        else if (barcodeOf.value(f.at(1)) != f.at(2))
            report.problems << QStringLiteral("%1: barcode %2 is not the barcode of %3 (%4).")
                .arg(where, f.at(2), f.at(1), barcodeOf.value(f.at(1)));
        if (!isPositiveCount(f.at(3)))
            report.problems << QStringLiteral("%1: quantity '%2' must be a whole number above 0.").arg(where, f.at(3));
    }
    for (const Record &r : orders)
        if (!withItems.contains(r.f.at(0)))
            report.problems << QStringLiteral("%1: order %2 has no items in OrderDetail.txt.")
                .arg(at(QStringLiteral("Order.txt"), r), r.f.at(0));
    return finish(report);
}

struct Season
{
    QDate start, end;
    double percent = 0;
};

QVector<Season> loadSeasons(const QString &projectDir, QStringList &problems, bool &found)
{
    QVector<Record> records;
    found = readFile(projectDir, QStringLiteral("Seasons.txt"), {4}, records, problems,
        QStringLiteral(" (start date, end date, percentage, isActive)"));
    QVector<Season> seasons;
    for (const Record &r : records)
    {
        const QString where = at(QStringLiteral("Seasons.txt"), r);
        const QStringList &f = r.f;
        bool ok = true;
        if (!isDate(f.at(0)))
        {
            problems << QStringLiteral("%1: start date '%2' must be yyyy-MM-dd.").arg(where, f.at(0));
            ok = false;
        }
        if (!isDate(f.at(1)))
        {
            problems << QStringLiteral("%1: end date '%2' must be yyyy-MM-dd.").arg(where, f.at(1));
            ok = false;
        }
        if (ok && QDate::fromString(f.at(0), Qt::ISODate) > QDate::fromString(f.at(1), Qt::ISODate))
            problems << QStringLiteral("%1: the start date is after the end date.").arg(where);
        if (!matches(f.at(2), "\\d+(\\.\\d+)?") || f.at(2).toDouble() <= 0 || f.at(2).toDouble() > 100)
        {
            problems << QStringLiteral("%1: discount percentage '%2' must be a number above 0 and at most 100, without a percent sign.")
                .arg(where, f.at(2));
            ok = false;
        }
        if (f.at(3) != QStringLiteral("true") && f.at(3) != QStringLiteral("false"))
            problems << QStringLiteral("%1: isActive '%2' must be true or false.").arg(where, f.at(3));
        if (ok)
            seasons.append(Season{QDate::fromString(f.at(0), Qt::ISODate),
                QDate::fromString(f.at(1), Qt::ISODate), f.at(2).toDouble()});
    }
    return seasons;
}

Result checkSeasons(const QString &projectDir, Report &report)
{
    bool found = false;
    loadSeasons(projectDir, report.problems, found);
    if (!found)
        return skip(report, QStringLiteral("Data/Seasons.txt does not exist yet (req 13)."));
    return finish(report);
}

// Bill.txt fields (req 8).
enum BillField
{
    BillId, Customer, OrderId, WithoutTax, Tax, Discount, Payable, Received, Balance,
    Status, Seasonal, Special, BillDate, BillTime
};

Result checkBills(const QString &projectDir, Report &report)
{
    QVector<Record> bills;
    if (!readFile(projectDir, QStringLiteral("Bill.txt"), {14}, bills, report.problems,
            QStringLiteral(" in the req 8 order")))
    {
        report.problems << QStringLiteral("Data/Bill.txt is missing.");
        return Result::Failed;
    }
    QStringList ignored;
    bool ordersFound = false;
    const QHash<QString, Record> orders = loadOrders(projectDir, ignored, ordersFound);
    bool seasonsFound = false;
    const QVector<Season> seasons = loadSeasons(projectDir, ignored, seasonsFound);

    QHash<QString, int> ids;
    const char *moneyNames[] = {"total without tax", "total tax", "total discount", "total payable",
        "amount received", "balance returned"};
    for (const Record &r : bills)
    {
        const QString where = at(QStringLiteral("Bill.txt"), r);
        const QStringList &f = r.f;
        if (!matches(f.at(BillId), "\\d{9}"))
            report.problems << QStringLiteral("%1: Bill Id '%2' must be 9 digits.").arg(where, f.at(BillId));
        else if (ids.contains(f.at(BillId)))
            report.problems << QStringLiteral("%1: Bill Id %2 repeats line %3 (req 8: unique).")
                .arg(where, f.at(BillId)).arg(ids.value(f.at(BillId)));
        ids.insert(f.at(BillId), r.line);

        if (!matches(f.at(OrderId), "\\d{8}"))
            report.problems << QStringLiteral("%1: Order Id '%2' (field 3) must be an 8-digit order id.").arg(where, f.at(OrderId));
        else if (!orders.contains(f.at(OrderId)))
            report.problems << QStringLiteral("%1: Order Id %2 is not in Order.txt.").arg(where, f.at(OrderId));

        bool moneyOk = true;
        for (int i = WithoutTax; i <= Balance; ++i)
            if (!isMoney(f.at(i)))
            {
                report.problems << QStringLiteral("%1: %2 '%3' (field %4) must be money with 2 decimals.")
                    .arg(where, QLatin1String(moneyNames[i - WithoutTax]), f.at(i)).arg(i + 1);
                moneyOk = false;
            }
        for (int i : {int(Seasonal), int(Special)})
            if (!isMoney(f.at(i)))
            {
                report.problems << QStringLiteral("%1: %2 discount '%3' (field %4) must be money with 2 decimals.")
                    .arg(where, i == Seasonal ? QStringLiteral("seasonal") : QStringLiteral("special"), f.at(i)).arg(i + 1);
                moneyOk = false;
            }

        const QString status = f.at(Status);
        if (!kOrderStatuses.contains(status))
            report.problems << QStringLiteral("%1: status '%2' (field 10) must be an order status (%3).")
                .arg(where, status, kOrderStatuses.join(QStringLiteral(", ")));
        else if (orders.contains(f.at(OrderId)) && orders.value(f.at(OrderId)).f.at(5) != status)
            report.problems << QStringLiteral("%1: status '%2' differs from order %3's status '%4' in Order.txt (req 8).")
                .arg(where, status, f.at(OrderId), orders.value(f.at(OrderId)).f.at(5));

        const bool dateOk = isDate(f.at(BillDate));
        if (!dateOk)
            report.problems << QStringLiteral("%1: bill date '%2' (field 13) must be yyyy-MM-dd.").arg(where, f.at(BillDate));
        if (!isTime(f.at(BillTime)))
            report.problems << QStringLiteral("%1: bill time '%2' (field 14) must be HH:mm:ss.").arg(where, f.at(BillTime));

        if (!moneyOk)
            continue;
        const double withoutTax = f.at(WithoutTax).toDouble();
        const double tax = f.at(Tax).toDouble();
        const double discount = f.at(Discount).toDouble();
        const double payable = f.at(Payable).toDouble();
        const double received = f.at(Received).toDouble();
        const double balance = f.at(Balance).toDouble();
        const double seasonal = f.at(Seasonal).toDouble();
        const double special = f.at(Special).toDouble();
        const double gross = withoutTax + tax;

        if (!sameMoney(payable, withoutTax + tax - discount - seasonal - special))
            report.problems << QStringLiteral(
                "%1: total payable %2 should be %3 (without tax %4 + tax %5 - discount %6 - seasonal %7 - special %8).")
                .arg(where, f.at(Payable), money(withoutTax + tax - discount - seasonal - special),
                    f.at(WithoutTax), f.at(Tax), f.at(Discount), f.at(Seasonal), f.at(Special));
        if (discount > withoutTax + 0.0001)
            report.problems << QStringLiteral("%1: total discount %2 is more than the bill %3.").arg(where, f.at(Discount), f.at(WithoutTax));
        if (special > gross * 0.08 + 0.005)
            report.problems << QStringLiteral("%1: special discount %2 is more than 8 percent of the bill (%3).")
                .arg(where, f.at(Special), money(gross * 0.08));
        if (status == QStringLiteral("paid"))
        {
            if (received + 0.0001 < payable)
                report.problems << QStringLiteral("%1: amount received %2 is less than the total payable %3.")
                    .arg(where, f.at(Received), f.at(Payable));
            else if (!sameMoney(balance, received - payable))
                report.problems << QStringLiteral("%1: balance returned %2 should be %3 (received %4 - payable %5).")
                    .arg(where, f.at(Balance), money(received - payable), f.at(Received), f.at(Payable));
        }
        if (seasonal > 0 && dateOk)
        {
            const QDate day = QDate::fromString(f.at(BillDate), Qt::ISODate);
            double best = -1;
            for (const Season &s : seasons)
                if (s.start <= day && day <= s.end)
                    best = std::max(best, s.percent);
            if (!seasonsFound)
                report.problems << QStringLiteral("%1: seasonal discount %2 but Data/Seasons.txt does not exist (req 13, 15).")
                    .arg(where, f.at(Seasonal));
            else if (best < 0)
                report.problems << QStringLiteral("%1: seasonal discount %2 but no season in Seasons.txt includes %3 (req 15).")
                    .arg(where, f.at(Seasonal), f.at(BillDate));
            else if (seasonal > gross * best / 100.0 + 0.005)
                report.problems << QStringLiteral("%1: seasonal discount %2 is more than %3 percent of the bill.")
                    .arg(where, f.at(Seasonal)).arg(best);
        }
    }
    return finish(report);
}

Result checkBillDetails(const QString &projectDir, Report &report)
{
    QVector<Record> details;
    if (!readFile(projectDir, QStringLiteral("BillDetail.txt"), {5}, details, report.problems,
            QStringLiteral(" (Bill Id, acronym, unit price, quantity, discount)")))
    {
        report.problems << QStringLiteral("Data/BillDetail.txt is missing (req 9).");
        return Result::Failed;
    }
    QStringList ignored;
    QVector<Record> bills;
    readFile(projectDir, QStringLiteral("Bill.txt"), {14}, bills, ignored);
    QHash<QString, Record> billById;
    for (const Record &r : bills)
        billById.insert(r.f.at(BillId), r);
    QSet<QString> acronyms;
    for (const Product &p : loadProducts(projectDir, ignored).products)
        acronyms.insert(p.acronym);

    QHash<QString, double> subtotal;
    QHash<QString, double> discount;
    for (const Record &r : details)
    {
        const QString where = at(QStringLiteral("BillDetail.txt"), r);
        const QStringList &f = r.f;
        if (!matches(f.at(0), "\\d{9}"))
            report.problems << QStringLiteral("%1: Bill Id '%2' must be 9 digits.").arg(where, f.at(0));
        else if (!billById.contains(f.at(0)))
            report.problems << QStringLiteral("%1: Bill Id %2 is not in Bill.txt.").arg(where, f.at(0));
        if (!acronyms.contains(f.at(1)))
            report.problems << QStringLiteral("%1: product acronym '%2' is not in Product.txt.").arg(where, f.at(1));
        const bool priceOk = isMoney(f.at(2));
        const bool quantityOk = isPositiveCount(f.at(3));
        const bool discountOk = isMoney(f.at(4));
        if (!priceOk)
            report.problems << QStringLiteral("%1: unit price '%2' must be money with 2 decimals.").arg(where, f.at(2));
        if (!quantityOk)
            report.problems << QStringLiteral("%1: quantity '%2' must be a whole number above 0.").arg(where, f.at(3));
        if (!discountOk)
            report.problems << QStringLiteral("%1: discount '%2' must be money with 2 decimals.").arg(where, f.at(4));
        if (!priceOk || !quantityOk || !discountOk)
            continue;
        const double lineTotal = f.at(2).toDouble() * f.at(3).toLongLong();
        if (f.at(4).toDouble() > lineTotal + 0.0001)
            report.problems << QStringLiteral("%1: discount %2 is more than the line total %3.").arg(where, f.at(4), money(lineTotal));
        subtotal[f.at(0)] += lineTotal;
        discount[f.at(0)] += f.at(4).toDouble();
    }
    for (const Record &b : bills)
    {
        const QString id = b.f.at(BillId);
        const QString where = at(QStringLiteral("Bill.txt"), b);
        if (!subtotal.contains(id))
        {
            report.problems << QStringLiteral("%1: bill %2 has no lines in BillDetail.txt (req 9).").arg(where, id);
            continue;
        }
        if (isMoney(b.f.at(WithoutTax)) && !sameMoney(subtotal.value(id), b.f.at(WithoutTax).toDouble()))
            report.problems << QStringLiteral("%1: total without tax %2, but its BillDetail lines add up to %3.")
                .arg(where, b.f.at(WithoutTax), money(subtotal.value(id)));
        if (isMoney(b.f.at(Discount)) && !sameMoney(discount.value(id), b.f.at(Discount).toDouble()))
            report.problems << QStringLiteral("%1: total discount %2, but its BillDetail discounts add up to %3.")
                .arg(where, b.f.at(Discount), money(discount.value(id)));
    }
    return finish(report);
}
}

QStringList groups()
{
    return {QStringLiteral("FileNames"), QStringLiteral("Users"), QStringLiteral("Products"),
        QStringLiteral("ProductCategories"), QStringLiteral("PCategory"), QStringLiteral("Orders"),
        QStringLiteral("Bills"), QStringLiteral("BillDetails"), QStringLiteral("Seasons")};
}

Result runGroup(const QString &group, const QString &projectDir, Report &report)
{
    report = Report{};
    if (group == QStringLiteral("FileNames"))
        return checkFileNames(projectDir, report);
    if (group == QStringLiteral("Users"))
        return checkUsers(projectDir, report);
    if (group == QStringLiteral("Products"))
        return checkProducts(projectDir, report);
    if (group == QStringLiteral("ProductCategories"))
        return checkProductCategories(projectDir, report);
    if (group == QStringLiteral("PCategory"))
        return checkPCategory(projectDir, report);
    if (group == QStringLiteral("Orders"))
        return checkOrders(projectDir, report);
    if (group == QStringLiteral("Bills"))
        return checkBills(projectDir, report);
    if (group == QStringLiteral("BillDetails"))
        return checkBillDetails(projectDir, report);
    if (group == QStringLiteral("Seasons"))
        return checkSeasons(projectDir, report);
    report.problems << QStringLiteral("Unknown check group '%1'.").arg(group);
    return Result::Failed;
}

Result runAll(const QString &projectDir, Report &report)
{
    Report all;
    QStringList skipped;
    for (const QString &group : groups())
    {
        Report one;
        const Result result = runGroup(group, projectDir, one);
        for (const QString &problem : one.problems)
            all.problems << QStringLiteral("[%1] %2").arg(group, problem);
        if (result == Result::Skipped)
            skipped << QStringLiteral("%1 (%2)").arg(group, one.skipReason);
    }
    all.skipReason = skipped.join(QStringLiteral("; "));
    report = all;
    return report.problems.isEmpty() ? Result::Passed : Result::Failed;
}
}
