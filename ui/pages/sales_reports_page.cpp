#include "sales_reports_page.h"

#include "../../POS/report/sales_report.h"

#include <QComboBox>
#include <QDateEdit>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace
{
QString money(double amount)
{
    return QStringLiteral("PKR %1").arg(amount, 0, 'f', 2);
}

class SalesReportsPage final : public QWidget
{
public:
    explicit SalesReportsPage(QWidget *parent) : QWidget(parent)
    {
        auto *root = new QVBoxLayout(this);
        auto *filters = new QFormLayout;
        period_ = new QComboBox(this);
        period_->addItems({QStringLiteral("Daily"), QStringLiteral("Weekly"),
            QStringLiteral("Monthly"), QStringLiteral("Yearly"),
            QStringLiteral("Custom range")});
        anchor_ = new QDateEdit(QDate::currentDate(), this);
        start_ = new QDateEdit(QDate::currentDate(), this);
        end_ = new QDateEdit(QDate::currentDate(), this);
        for (QDateEdit *edit : {anchor_, start_, end_})
        {
            edit->setCalendarPopup(true);
            edit->setDisplayFormat(QStringLiteral("dd MMM yyyy"));
        }
        filters->addRow(QStringLiteral("Period"), period_);
        filters->addRow(QStringLiteral("Date"), anchor_);
        filters->addRow(QStringLiteral("From"), start_);
        filters->addRow(QStringLiteral("To"), end_);
        root->addLayout(filters);
        auto *runButton = new QPushButton(QStringLiteral("Run reports"), this);
        root->addWidget(runButton);

        auto *grid = new QGridLayout;
        cashOrders_ = new QLabel(this);
        cashSales_ = new QLabel(this);
        allOrders_ = new QLabel(this);
        allSales_ = new QLabel(this);
        discounts_ = new QLabel(this);
        grid->addWidget(new QLabel(QStringLiteral("Cash orders"), this), 0, 0);
        grid->addWidget(cashOrders_, 0, 1);
        grid->addWidget(new QLabel(QStringLiteral("Cash sales"), this), 1, 0);
        grid->addWidget(cashSales_, 1, 1);
        grid->addWidget(new QLabel(QStringLiteral("All paid orders"), this), 2, 0);
        grid->addWidget(allOrders_, 2, 1);
        grid->addWidget(new QLabel(QStringLiteral("All sales"), this), 3, 0);
        grid->addWidget(allSales_, 3, 1);
        grid->addWidget(new QLabel(QStringLiteral("Total discounts"), this), 4, 0);
        grid->addWidget(discounts_, 4, 1);
        root->addLayout(grid);
        status_ = new QLabel(this);
        root->addWidget(status_);
        root->addStretch();

        connect(period_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() {
            updateDateControls();
        });
        connect(runButton, &QPushButton::clicked, this, [this]() { refresh(); });
        updateDateControls();
        refresh();
    }

private:
    void updateDateControls()
    {
        const bool custom = period_->currentIndex() == 4;
        anchor_->setVisible(!custom);
        start_->setVisible(custom);
        end_->setVisible(custom);
    }

    void refresh()
    {
        const Pos::SalesReportPeriod periods[] = {
            Pos::SalesReportPeriod::Daily, Pos::SalesReportPeriod::Weekly,
            Pos::SalesReportPeriod::Monthly, Pos::SalesReportPeriod::Yearly,
            Pos::SalesReportPeriod::Custom
        };
        QDate from, to;
        QString error;
        if (!Pos::reportDateRange(periods[period_->currentIndex()], anchor_->date(),
                start_->date(), end_->date(), from, to, error))
        {
            status_->setText(error);
            return;
        }
        Pos::SalesReport cash;
        Pos::SalesReport all;
        if (!Pos::cashSalesReport(from, to, cash, error)
            || !Pos::allSalesReport(from, to, all, error))
        {
            status_->setText(error);
            return;
        }
        cashOrders_->setText(QString::number(cash.orderCount));
        cashSales_->setText(money(cash.totalSales));
        allOrders_->setText(QString::number(all.orderCount));
        allSales_->setText(money(all.totalSales));
        discounts_->setText(money(all.totalDiscounts));
        status_->setText(QStringLiteral("%1 to %2")
            .arg(from.toString(QStringLiteral("dd MMM yyyy")),
                 to.toString(QStringLiteral("dd MMM yyyy"))));
    }

    QComboBox *period_ = nullptr;
    QDateEdit *anchor_ = nullptr;
    QDateEdit *start_ = nullptr;
    QDateEdit *end_ = nullptr;
    QLabel *cashOrders_ = nullptr;
    QLabel *cashSales_ = nullptr;
    QLabel *allOrders_ = nullptr;
    QLabel *allSales_ = nullptr;
    QLabel *discounts_ = nullptr;
    QLabel *status_ = nullptr;
};
}

QWidget *UiPages::createSalesReportsPage(QWidget *parent)
{
    return new SalesReportsPage(parent);
}
