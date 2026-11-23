#include "seasons_page.h"

#include "../../POS/season/season_service.h"

#include <QCheckBox>
#include <QDateEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace
{
bool editSeason(QWidget *parent, const Pos::Season *existing, Pos::Season &season)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(existing ? QStringLiteral("Edit season")
                                   : QStringLiteral("Add season"));
    auto *layout = new QFormLayout(&dialog);
    auto *name = new QLineEdit(&dialog);
    auto *start = new QDateEdit(QDate::currentDate(), &dialog);
    auto *end = new QDateEdit(QDate::currentDate(), &dialog);
    auto *percentage = new QDoubleSpinBox(&dialog);
    auto *active = new QCheckBox(QStringLiteral("Active"), &dialog);
    start->setCalendarPopup(true);
    end->setCalendarPopup(true);
    start->setDisplayFormat(QStringLiteral("dd MMM yyyy"));
    end->setDisplayFormat(QStringLiteral("dd MMM yyyy"));
    percentage->setRange(0.01, 100.0);
    percentage->setDecimals(2);
    percentage->setSuffix(QStringLiteral(" %"));
    active->setChecked(true);
    if (existing)
    {
        name->setText(existing->name);
        start->setDate(existing->startDate);
        end->setDate(existing->endDate);
        percentage->setValue(existing->percentage);
        active->setChecked(existing->isActive);
    }
    layout->addRow(QStringLiteral("Season name"), name);
    layout->addRow(QStringLiteral("Start date"), start);
    layout->addRow(QStringLiteral("End date"), end);
    layout->addRow(QStringLiteral("Discount"), percentage);
    layout->addRow(QString(), active);
    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    layout->addRow(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted,
        &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected,
        &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted)
        return false;
    season.name = name->text().trimmed();
    season.startDate = start->date();
    season.endDate = end->date();
    season.percentage = percentage->value();
    season.isActive = active->isChecked();
    return true;
}

class SeasonsPage final : public QWidget
{
public:
    explicit SeasonsPage(QWidget *parent) : QWidget(parent)
    {
        auto *root = new QVBoxLayout(this);
        auto *header = new QHBoxLayout;
        title_ = new QLabel(QStringLiteral("Season Management"), this);
        title_->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: 700;"));
        count_ = new QLabel(this);
        header->addWidget(title_);
        header->addStretch();
        header->addWidget(count_);
        root->addLayout(header);

        auto *actions = new QHBoxLayout;
        auto *add = new QPushButton(QStringLiteral("Add season"), this);
        edit_ = new QPushButton(QStringLiteral("Edit"), this);
        toggle_ = new QPushButton(QStringLiteral("Activate / Deactivate"), this);
        remove_ = new QPushButton(QStringLiteral("Delete"), this);
        actions->addWidget(add);
        actions->addWidget(edit_);
        actions->addWidget(toggle_);
        actions->addWidget(remove_);
        actions->addStretch();
        root->addLayout(actions);

        table_ = new QTableWidget(this);
        table_->setColumnCount(5);
        table_->setHorizontalHeaderLabels({QStringLiteral("Season"),
            QStringLiteral("Start"), QStringLiteral("End"),
            QStringLiteral("Discount"), QStringLiteral("Status")});
        table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        table_->verticalHeader()->hide();
        table_->setSelectionBehavior(QAbstractItemView::SelectRows);
        table_->setSelectionMode(QAbstractItemView::SingleSelection);
        table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        root->addWidget(table_, 1);
        status_ = new QLabel(this);
        root->addWidget(status_);

        connect(add, &QPushButton::clicked, this, [this]() { addSeason(); });
        connect(edit_, &QPushButton::clicked, this, [this]() { editSelected(); });
        connect(toggle_, &QPushButton::clicked, this, [this]() { toggleSelected(); });
        connect(remove_, &QPushButton::clicked, this, [this]() { deleteSelected(); });
        connect(table_, &QTableWidget::itemSelectionChanged, this, [this]() {
            const bool selected = selectedIndex() >= 0;
            edit_->setEnabled(selected);
            toggle_->setEnabled(selected);
            remove_->setEnabled(selected);
        });
        reload();
    }

private:
    int selectedIndex() const
    {
        const auto rows = table_->selectionModel()->selectedRows();
        return rows.isEmpty() ? -1 : rows.first().row();
    }

    void reload()
    {
        QString error;
        if (!Pos::getAllSeasons(seasons_, error))
        {
            QMessageBox::critical(this, QStringLiteral("Season data error"), error);
            return;
        }
        table_->setRowCount(seasons_.size());
        int active = 0;
        for (int i = 0; i < seasons_.size(); ++i)
        {
            const Pos::Season &season = seasons_.at(i);
            const QString values[] = {
                season.name,
                season.startDate.toString(QStringLiteral("dd MMM yyyy")),
                season.endDate.toString(QStringLiteral("dd MMM yyyy")),
                QStringLiteral("%1%").arg(season.percentage, 0, 'f', 2),
                season.isActive ? QStringLiteral("Active") : QStringLiteral("Inactive")
            };
            for (int col = 0; col < 5; ++col)
                table_->setItem(i, col, new QTableWidgetItem(values[col]));
            if (season.isActive)
                ++active;
        }
        count_->setText(QStringLiteral("%1 seasons (%2 active)")
            .arg(seasons_.size()).arg(active));
        status_->setText(QStringLiteral("Changes are saved to Data/Seasons.txt."));
    }

    void addSeason()
    {
        Pos::Season season;
        if (!editSeason(this, nullptr, season))
            return;
        QString error;
        if (!Pos::addSeason(season, error))
        {
            QMessageBox::warning(this, QStringLiteral("Cannot add season"), error);
            return;
        }
        reload();
    }

    void editSelected()
    {
        const int index = selectedIndex();
        if (index < 0)
            return;
        Pos::Season season = seasons_.at(index);
        if (!editSeason(this, &seasons_.at(index), season))
            return;
        QString error;
        if (!Pos::updateSeason(index, season, error))
        {
            QMessageBox::warning(this, QStringLiteral("Cannot update season"), error);
            return;
        }
        reload();
    }

    void toggleSelected()
    {
        const int index = selectedIndex();
        if (index < 0)
            return;
        QString error;
        if (!Pos::setSeasonActive(index, !seasons_.at(index).isActive, error))
        {
            QMessageBox::warning(this, QStringLiteral("Cannot change season status"), error);
            return;
        }
        reload();
    }

    void deleteSelected()
    {
        const int index = selectedIndex();
        if (index < 0)
            return;
        if (QMessageBox::question(this, QStringLiteral("Delete season"),
                QStringLiteral("Delete '%1'?").arg(seasons_.at(index).name))
            != QMessageBox::Yes)
            return;
        QString error;
        if (!Pos::deleteSeason(index, error))
        {
            QMessageBox::warning(this, QStringLiteral("Cannot delete season"), error);
            return;
        }
        reload();
    }

    QVector<Pos::Season> seasons_;
    QLabel *title_ = nullptr;
    QLabel *count_ = nullptr;
    QLabel *status_ = nullptr;
    QTableWidget *table_ = nullptr;
    QPushButton *edit_ = nullptr;
    QPushButton *toggle_ = nullptr;
    QPushButton *remove_ = nullptr;
};
}

QWidget *UiPages::createSeasonsPage(QWidget *parent)
{
    return new SeasonsPage(parent);
}
