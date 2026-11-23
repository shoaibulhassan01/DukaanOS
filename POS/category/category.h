#pragma once

#include <QString>

namespace Pos
{
// Owner: Umar Khan - one product category, one line of PCategory.txt
// (Increment 3, requirement 1):  Id Name Acronym#   e.g.  10 Beverages BEV#
// The Id is 2 digits and is the first two digits of every barcode in the
// category.
struct Category
{
    QString id;        // exactly 2 digits, e.g. "10" or "01"
    QString name;      // one word in the file: spaces are stored as '_'
    QString acronym;   // one word in the file: spaces are stored as '_'
};
}
