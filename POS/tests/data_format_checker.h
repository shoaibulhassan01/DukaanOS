#pragma once

// Owner: Umar Khan - checks the NAMES and FORMATS of the data files against
// the Increment 3 requirements, line by line. It reads the files itself and
// does not use the team's read/save functions, so a bug in those cannot hide
// a wrong file.
//
// Only rules stated in the requirements (or the formats already used by the
// earlier increments, "space separated ... as before") are checked. Where the
// requirements leave something open, the check accepts every reasonable
// reading (see the notes in data_format_checker.cpp).
//
// A group whose file does not exist yet (PCategory.txt, Seasons.txt) is
// SKIPPED, not passed: CI stays green and the log shows it as not done yet.
// As soon as the file appears, every rule for it applies.

#include <QString>
#include <QStringList>

namespace DataFormat
{
enum class Result
{
    Passed,
    Failed,
    Skipped
};

struct Report
{
    QStringList problems;   // each one makes the group fail
    QString skipReason;     // set when the group is skipped
};

// FileNames, Users, Products, ProductCategories, PCategory, Orders, Bills,
// BillDetails, Seasons.
QStringList groups();

// projectDir: the folder that holds Data/ (barcode image paths such as
// "Data/barcodes/100001.png" are relative to it).
Result runGroup(const QString &group, const QString &projectDir, Report &report);

// Every group; Failed if any group failed. Skipped groups are listed in
// report.skipReason. Used by the UI test on the files the app has written.
Result runAll(const QString &projectDir, Report &report);
}
