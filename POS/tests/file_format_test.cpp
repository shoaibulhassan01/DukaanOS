// Owner: Umar Khan - runs one group of the data-file name/format checks
// (data_format_checker.h) on the project's Data folder.
//
//   FileFormatTest <group> [project folder]
//
// Exit code 0 = passed, 1 = failed, 77 = skipped (the file for that
// requirement does not exist yet). CMakeLists.txt registers one CTest test per
// group and tells CTest that 77 means "Skipped", so CI stays green for parts
// that are not built yet and shows them as skipped.

#include "data_format_checker.h"

#include <QCoreApplication>
#include <QTextStream>

#ifndef DUKAANOS_SOURCE_DIR
#error "DUKAANOS_SOURCE_DIR must point at the project folder (set in CMakeLists.txt)."
#endif

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments();
    QTextStream out(stdout);
    QTextStream err(stderr);
    if (args.size() < 2 || !DataFormat::groups().contains(args.at(1)))
    {
        err << "Usage: FileFormatTest <" << DataFormat::groups().join(QLatin1Char('|'))
            << "> [project folder]" << Qt::endl;
        return 1;
    }
    const QString group = args.at(1);
    const QString project = args.size() > 2 ? args.at(2) : QStringLiteral(DUKAANOS_SOURCE_DIR);

    DataFormat::Report report;
    switch (DataFormat::runGroup(group, project, report))
    {
    case DataFormat::Result::Passed:
        out << group << ": file names and formats are correct." << Qt::endl;
        return 0;
    case DataFormat::Result::Skipped:
        out << group << ": SKIPPED - " << report.skipReason << Qt::endl;
        return 77;
    case DataFormat::Result::Failed:
        break;
    }
    for (const QString &problem : report.problems)
        err << "FAIL: " << problem << Qt::endl;
    err << group << ": " << report.problems.size() << " problem(s)." << Qt::endl;
    return 1;
}
