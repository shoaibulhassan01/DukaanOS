#pragma once

class QWidget;

namespace UiPages
{
// Manager reports (Increment 3, requirement 11), shown as tables. One page
// for all reports: pick a report from the list at the top.
QWidget *createReportsPage(QWidget *parent = nullptr);
}
