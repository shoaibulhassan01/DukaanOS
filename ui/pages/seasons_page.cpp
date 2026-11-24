
#include "seasons_page.h"
#include "../theme.h"

#include "../../POS/season/season.h"
#include "../../POS/season/season_service.h"

#include <QCheckBox>
#include <QDateEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QVector>

namespace
{
const QString kNavy = QStringLiteral("#111184");
const QString kInk = QStringLiteral("#20233B");
const QString kMuted = QStringLiteral("#777B91");
const QString kLine = QStringLiteral("#E7E9F3");

QFrame *createCard(QWidget *parent)
{
    auto *card = new QFrame(parent);
    card->setObjectName(QStringLiteral("seasonsCard"));
    card->setStyleSheet(QStringLiteral(
        "QFrame#seasonsCard { background: white; border: 1px solid %1; border-radius: 14px; }")
        .arg(kLine));

    return card;
}

QFrame *createStatCard(QWidget *parent, const QString &heading,
    const QString &accent, QLabel **value, const QString &hint)
{
    auto *card = createCard(parent);

    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(17, 15, 17, 15);
    layout->setSpacing(4);

    auto *title = new QLabel(heading, card);
    title->setStyleSheet(QStringLiteral(
        "color: %1; font-size: 10px; font-weight: 600; background: transparent;")
        .arg(kMuted));

    *value = new QLabel(QStringLiteral("0"), card);
    (*value)->setStyleSheet(QStringLiteral(
        "color: %1; font-size: 24px; font-weight: 700; background: transparent;")
        .arg(kInk));

    auto *subheading = new QLabel(hint, card);
    subheading->setStyleSheet(QStringLiteral(
        "color: %1; font-size: 9px; font-weight: 600; background: transparent;")
        .arg(accent));

    layout->addWidget(title);
    layout->addWidget(*value);
    layout->addWidget(subheading);

    return card;
}

bool showSeasonDialog(QWidget *parent, const Pos::Season *existing,
    Pos::Season &result)
{
    QDialog dialog(parent);

    dialog.setWindowTitle(existing
        ? QStringLiteral("Edit Season")
        : QStringLiteral("Add Season"));

    dialog.setMinimumWidth(380);

    auto *layout = new QVBoxLayout(&dialog);
    auto *form = new QFormLayout;

    auto *startDate = new QDateEdit(QDate::currentDate(), &dialog);
    startDate->setCalendarPopup(true);
    startDate->setDisplayFormat(QStringLiteral("dd MMM yyyy"));

    auto *endDate = new QDateEdit(QDate::currentDate().addMonths(1), &dialog);
    endDate->setCalendarPopup(true);
    endDate->setDisplayFormat(QStringLiteral("dd MMM yyyy"));

    auto *percentage = new QDoubleSpinBox(&dialog);
    percentage->setRange(0.01, 100.00);
    percentage->setDecimals(2);
    percentage->setSuffix(QStringLiteral(" %"));

    auto *active = new QCheckBox(QStringLiteral("Active"), &dialog);
    active->setChecked(true);

    if (existing)
    {
        startDate->setDate(existing->startDate);
        endDate->setDate(existing->endDate);
        percentage->setValue(existing->percentage);
        active->setChecked(existing->isActive);
    }

    form->addRow(QStringLiteral("Start Date:"), startDate);
    form->addRow(QStringLiteral("End Date:"), endDate);
    form->addRow(QStringLiteral("Discount:"), percentage);
    form->addRow(QStringLiteral("Status:"), active);

    layout->addLayout(form);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        &dialog);

    layout->addWidget(buttons);

    QObject::connect(buttons, &QDialogButtonBox::accepted,
        &dialog, &QDialog::accept);

    QObject::connect(buttons, &QDialogButtonBox::rejected,
        &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted)
        return false;

    result.startDate = startDate->date();
    result.endDate = endDate->date();
    result.percentage = percentage->value();
    result.isActive = active->isChecked();

    return true;
}

class SeasonsPage : public QWidget
{
public:
    explicit SeasonsPage(QWidget *parent)
        : QWidget(parent)
    {
        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 18);
        root->setSpacing(16);

        auto *stats = new QHBoxLayout;
        stats->setSpacing(12);

        stats->addWidget(createStatCard(this,
            QStringLiteral("TOTAL SEASONS"),
            QStringLiteral("#4F46E5"),
            &totalCount_,
            QStringLiteral("All saved seasons")), 1);

        stats->addWidget(createStatCard(this,
            QStringLiteral("ACTIVE SEASONS"),
            QStringLiteral("#16845B"),
            &activeCount_,
            QStringLiteral("Currently enabled")), 1);

        root->addLayout(stats);

        auto *card = createCard(this);
        auto *layout = new QVBoxLayout(card);

        layout->setContentsMargins(18, 18, 18, 16);
        layout->setSpacing(13);

        auto *heading = new QHBoxLayout;

        auto *title = new QLabel(
            QStringLiteral("Season Management"), card);

        title->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 15px; font-weight: 700; background: transparent;")
            .arg(kInk));

        heading->addWidget(title);
        heading->addStretch(1);

        countLabel_ = new QLabel(card);
        countLabel_->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 10px; background: transparent;")
            .arg(kMuted));

        heading->addWidget(countLabel_);

        editButton_ = new QPushButton(
            QStringLiteral("Edit Selected"), card);

        deleteButton_ = new QPushButton(
            QStringLiteral("Delete"), card);

        toggleButton_ = new QPushButton(
            QStringLiteral("Activate / Deactivate"), card);

        auto *addButton = new QPushButton(
            QStringLiteral("+ Add Season"), card);

        editButton_->setEnabled(false);
        deleteButton_->setEnabled(false);
        toggleButton_->setEnabled(false);

        auto setButtonStyle = [](QPushButton *button,
            const QString &background,
            const QString &foreground)
        {
            button->setCursor(Qt::PointingHandCursor);
            button->setFixedHeight(36);

            button->setStyleSheet(QStringLiteral(
                "QPushButton { background: %1; color: %2;"
                " border: 1px solid %3; border-radius: 8px;"
                " padding: 0 12px; font-size: 10px; font-weight: 600; }"
                "QPushButton:hover { opacity: 0.8; }"
                "QPushButton:disabled { color: #A9ABBD;"
                " background: #F6F6FA; border-color: %3; }")
                .arg(background, foreground, kLine));
        };

        setButtonStyle(editButton_, QStringLiteral("#F0F0FF"), kNavy);
        setButtonStyle(deleteButton_, QStringLiteral("#FEF3F2"),
            QStringLiteral("#B42318"));
        setButtonStyle(toggleButton_, QStringLiteral("#F0FDF4"),
            QStringLiteral("#16845B"));

        addButton->setCursor(Qt::PointingHandCursor);
        addButton->setFixedHeight(36);
        addButton->setStyleSheet(QStringLiteral(
            "QPushButton { background: %1; color: white;"
            " border: none; border-radius: 8px;"
            " padding: 0 14px; font-size: 10px; font-weight: 700; }"
            "QPushButton:hover { background: #0D0D6B; }")
            .arg(kNavy));

        heading->addWidget(editButton_);
        heading->addWidget(toggleButton_);
        heading->addWidget(deleteButton_);
        heading->addWidget(addButton);

        layout->addLayout(heading);

        table_ = new QTableWidget(card);
        table_->setColumnCount(4);

        table_->setHorizontalHeaderLabels({
            QStringLiteral("START DATE"),
            QStringLiteral("END DATE"),
            QStringLiteral("DISCOUNT"),
            QStringLiteral("STATUS")
        });

        table_->verticalHeader()->hide();

        table_->horizontalHeader()->setSectionResizeMode(
            0, QHeaderView::Stretch);

        for (int column = 1; column < 4; ++column)
        {
            table_->horizontalHeader()->setSectionResizeMode(
                column, QHeaderView::ResizeToContents);
        }

        table_->horizontalHeader()->setFixedHeight(40);

        table_->setShowGrid(false);
        table_->setSortingEnabled(false);

        table_->setEditTriggers(
            QAbstractItemView::NoEditTriggers);

        table_->setSelectionBehavior(
            QAbstractItemView::SelectRows);

        table_->setSelectionMode(
            QAbstractItemView::SingleSelection);

        table_->setStyleSheet(QStringLiteral(
            "QTableWidget { background: white; border: none;"
            " color: %1; outline: 0; }"
            "QTableWidget::item { padding: 5px 8px;"
            " border-bottom: 1px solid #F0F1F6; }"
            "QTableWidget::item:selected { background: #F0F0FF;"
            " color: %2; }"
            "QHeaderView::section { background: #F8F8FC;"
            " color: %3; border: none;"
            " border-bottom: 1px solid %4;"
            " padding: 0 8px; font-size: 9px; font-weight: 700; }")
            .arg(kInk, kNavy, kMuted, kLine));

        layout->addWidget(table_, 1);

        emptyLabel_ = new QLabel(card);
        emptyLabel_->setAlignment(Qt::AlignCenter);
        emptyLabel_->setWordWrap(true);
        emptyLabel_->setMinimumHeight(150);

        emptyLabel_->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 12px;"
            " background: #FAFAFD;"
            " border: 1px dashed %2;"
            " border-radius: 12px; padding: 18px;")
            .arg(kMuted, kLine));

        layout->addWidget(emptyLabel_);

        root->addWidget(card, 1);

        note_ = new QLabel(this);

        note_->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 10px; background: transparent;")
            .arg(kMuted));

        root->addWidget(note_);

        connect(table_, &QTableWidget::itemSelectionChanged,
            this, [this]()
        {
            const bool selected = selectedSeasonIndex() >= 0;

            editButton_->setEnabled(selected);
            deleteButton_->setEnabled(selected);
            toggleButton_->setEnabled(selected);
        });

        connect(table_, &QTableWidget::cellDoubleClicked,
            this, [this](int, int)
        {
            editSelected();
        });

        connect(addButton, &QPushButton::clicked,
            this, [this]()
        {
            addSeason();
        });

        connect(editButton_, &QPushButton::clicked,
            this, [this]()
        {
            editSelected();
        });

        connect(deleteButton_, &QPushButton::clicked,
            this, [this]()
        {
            deleteSelected();
        });

        connect(toggleButton_, &QPushButton::clicked,
            this, [this]()
        {
            toggleSelected();
        });

        loadSeasons();
    }

private:

    void loadSeasons()
    {
        QString error;

        if (!Pos::getAllSeasons(seasons_, error))
        {
            note_->setText(error);
            note_->setStyleSheet(
                QStringLiteral("color: #B42318; font-size: 10px;"));

            table_->hide();

            emptyLabel_->setText(
                QStringLiteral("Seasons could not be loaded."));

            emptyLabel_->show();
            return;
        }

        int activeCount = 0;

        for (const Pos::Season &season : seasons_)
        {
            if (season.isActive)
                activeCount++;
        }

        totalCount_->setText(QString::number(seasons_.size()));
        activeCount_->setText(QString::number(activeCount));

        note_->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 10px; background: transparent;")
            .arg(kMuted));

        note_->setText(
            QStringLiteral("Season information is loaded from Seasons.txt."));

        refreshTable();
    }

    void refreshTable()
    {
        table_->clearSelection();
        table_->setRowCount(0);
        table_->setRowCount(seasons_.size());

        for (int row = 0; row < seasons_.size(); ++row)
        {
            const Pos::Season &season = seasons_.at(row);

            setCell(row, 0,
                season.startDate.toString(
                    QStringLiteral("dd MMM yyyy")));

            setCell(row, 1,
                season.endDate.toString(
                    QStringLiteral("dd MMM yyyy")));

            setCell(row, 2,
                QStringLiteral("%1%").arg(
                    season.percentage, 0, 'f', 2));

            auto *status = new QTableWidgetItem(
                season.isActive
                    ? QStringLiteral("Active")
                    : QStringLiteral("Inactive"));

            status->setForeground(
                UiTheme::textColor(QColor(
                    season.isActive
                        ? QStringLiteral("#16845B")
                        : QStringLiteral("#777B91"))));

            status->setTextAlignment(Qt::AlignCenter);

            table_->setItem(row, 3, status);

            table_->setRowHeight(row, 48);
        }

        const bool hasSeasons = !seasons_.isEmpty();

        table_->setVisible(hasSeasons);
        emptyLabel_->setVisible(!hasSeasons);

        if (!hasSeasons)
        {
            emptyLabel_->setText(
                QStringLiteral("No seasons have been added yet."));
        }

        countLabel_->setText(
            QStringLiteral("%1 seasons").arg(seasons_.size()));
    }

    void addSeason()
    {
        Pos::Season season;

        if (!showSeasonDialog(this, nullptr, season))
            return;

        QString error;

        if (!Pos::addSeason(season, error))
        {
            QMessageBox::warning(
                this, QStringLiteral("Cannot Add Season"), error);

            return;
        }

        loadSeasons();
        showSaved(QStringLiteral("Season added successfully."));
    }

    void editSelected()
    {
        const int index = selectedSeasonIndex();

        if (index < 0)
            return;

        Pos::Season updated = seasons_.at(index);

        if (!showSeasonDialog(this, &seasons_.at(index), updated))
            return;

        QString error;

        if (!Pos::updateSeason(index, updated, error))
        {
            QMessageBox::warning(
                this, QStringLiteral("Cannot Update Season"), error);

            return;
        }

        loadSeasons();
        showSaved(QStringLiteral("Season updated successfully."));
    }

    void deleteSelected()
    {
        const int index = selectedSeasonIndex();

        if (index < 0)
            return;

        const Pos::Season season = seasons_.at(index);

        const auto answer = QMessageBox::question(
            this,
            QStringLiteral("Delete Season"),
            QStringLiteral("Delete the season from %1 to %2?\nThis cannot be undone.")
                .arg(season.startDate.toString(Qt::ISODate),
                     season.endDate.toString(Qt::ISODate)),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);

        if (answer != QMessageBox::Yes)
            return;

        QString error;

        if (!Pos::deleteSeason(index, error))
        {
            QMessageBox::warning(
                this, QStringLiteral("Cannot Delete Season"), error);

            return;
        }

        loadSeasons();
        showSaved(QStringLiteral("Season deleted successfully."));
    }

    void toggleSelected()
    {
        const int index = selectedSeasonIndex();

        if (index < 0)
            return;

        const bool newStatus = !seasons_.at(index).isActive;

        QString error;

        if (!Pos::setSeasonActive(index, newStatus, error))
        {
            QMessageBox::warning(
                this, QStringLiteral("Cannot Change Status"), error);

            return;
        }

        loadSeasons();

        showSaved(newStatus
            ? QStringLiteral("Season activated successfully.")
            : QStringLiteral("Season deactivated successfully."));
    }

    int selectedSeasonIndex() const
    {
        const QModelIndexList rows = table_->selectionModel()
            ? table_->selectionModel()->selectedRows()
            : QModelIndexList();

        if (rows.isEmpty() || rows.first().row() >= seasons_.size())
            return -1;

        return rows.first().row();
    }

    void setCell(int row, int column, const QString &value)
    {
        auto *cell = new QTableWidgetItem(value);

        cell->setFlags(cell->flags() & ~Qt::ItemIsEditable);

        table_->setItem(row, column, cell);
    }

    void showSaved(const QString &message)
    {
        note_->setStyleSheet(QStringLiteral(
            "color: #18764A; font-size: 10px;"
            " font-weight: 600; background: transparent;"));

        note_->setText(message);
    }

    QVector<Pos::Season> seasons_;

    QLabel *totalCount_ = nullptr;
    QLabel *activeCount_ = nullptr;
    QLabel *countLabel_ = nullptr;
    QLabel *emptyLabel_ = nullptr;
    QLabel *note_ = nullptr;

    QPushButton *editButton_ = nullptr;
    QPushButton *deleteButton_ = nullptr;
    QPushButton *toggleButton_ = nullptr;

    QTableWidget *table_ = nullptr;
};
}

QWidget *UiPages::createSeasonsPage(QWidget *parent)
{
    return new SeasonsPage(parent);
}
