
#include "seasons_page.h"
#include "../theme.h"

#include "../../POS/season/season.h"
#include "../../POS/season/season_service.h"

#include <QCheckBox>
#include <QDateEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleValidator>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QLineEdit>
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

// Add / Edit season form, styled like the other forms (Add category, Add
// product): labels above fields, rounded inputs, Cancel / Save buttons.
// Returns true if the manager saved; the caller checks and stores it.
bool showSeasonDialog(QWidget *parent, const Pos::Season *existing,
    Pos::Season &result)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(existing
        ? QStringLiteral("Edit season")
        : QStringLiteral("Add season"));
    dialog.setMinimumWidth(420);
    dialog.setStyleSheet(QStringLiteral(
        "QDialog { background: white; }"
        "QDateEdit, QLineEdit { background: #FAFAFD; color: %1; border: 1px solid %2;"
        " border-radius: 8px; padding: 0 10px; font-size: 12px; }"
        "QDateEdit:focus, QLineEdit:focus { background: white; border: 1px solid %3; }"
        "QDateEdit::drop-down { subcontrol-origin: padding; subcontrol-position: center right;"
        " width: 28px; border: none; background: transparent; }"
        "QDateEdit::down-arrow { image: url(:/assets/combo_arrow.png); width: 12px; height: 8px; }")
        .arg(kInk, kLine, kNavy));

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(22, 20, 22, 18);
    layout->setSpacing(8);

    auto *title = new QLabel(dialog.windowTitle(), &dialog);
    title->setStyleSheet(QStringLiteral(
        "color: %1; font-size: 15px; font-weight: 700;").arg(kInk));
    layout->addWidget(title);
    auto *hint = new QLabel(QStringLiteral(
        "While a season is active and today is between its dates, its discount "
        "is applied to every bill."), &dialog);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color: %1; font-size: 10px;").arg(kMuted));
    layout->addWidget(hint);
    layout->addSpacing(4);

    auto caption = [&dialog](const QString &text) {
        auto *label = new QLabel(text, &dialog);
        label->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 11px; font-weight: 600;").arg(kInk));
        return label;
    };
    auto dateBox = [&dialog](const QDate &date) {
        auto *box = new QDateEdit(date, &dialog);
        box->setCalendarPopup(true);
        box->setDisplayFormat(QStringLiteral("dd MMM yyyy"));
        box->setFixedHeight(38);
        return box;
    };

    // Start and end date side by side.
    auto *dates = new QGridLayout;
    dates->setHorizontalSpacing(12);
    dates->setVerticalSpacing(6);
    auto *startDate = dateBox(QDate::currentDate());
    auto *endDate = dateBox(QDate::currentDate().addMonths(1));
    dates->addWidget(caption(QStringLiteral("Start date")), 0, 0);
    dates->addWidget(caption(QStringLiteral("End date")), 0, 1);
    dates->addWidget(startDate, 1, 0);
    dates->addWidget(endDate, 1, 1);
    layout->addLayout(dates);

    layout->addWidget(caption(QStringLiteral("Discount (%)")));
    auto *percentage = new QLineEdit(&dialog);
    percentage->setObjectName(QStringLiteral("seasonDiscountPercentage"));
    auto *percentageValidator = new QDoubleValidator(0.01, 100.00, 2, percentage);
    percentageValidator->setNotation(QDoubleValidator::StandardNotation);
    percentageValidator->setLocale(QLocale::c());
    percentage->setValidator(percentageValidator);
    percentage->setPlaceholderText(QStringLiteral("e.g. 10.00"));
    percentage->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    percentage->setFixedHeight(38);
    layout->addWidget(percentage);

    layout->addSpacing(4);
    auto *active = new QCheckBox(QStringLiteral("Active"), &dialog);
    active->setChecked(true);
    active->setCursor(Qt::PointingHandCursor);
    active->setStyleSheet(QStringLiteral(
        "QCheckBox { color: %1; font-size: 11px; font-weight: 600; spacing: 8px; }"
        "QCheckBox::indicator { width: 18px; height: 18px; }"
        "QCheckBox::indicator:unchecked { background: white; border: 1px solid %2;"
        " border-radius: 4px; }"
        "QCheckBox::indicator:checked { background: %3; border: 1px solid %3;"
        " border-radius: 4px; image: url(:/assets/checkbox_check.png); }")
        .arg(kInk, kLine, kNavy));
    layout->addWidget(active);

    if (existing)
    {
        startDate->setDate(existing->startDate);
        endDate->setDate(existing->endDate);
        percentage->setText(QString::number(existing->percentage, 'f', 2));
        active->setChecked(existing->isActive);
    }

    auto *buttons = new QHBoxLayout;
    buttons->addStretch(1);
    auto *cancel = new QPushButton(QStringLiteral("Cancel"), &dialog);
    cancel->setCursor(Qt::PointingHandCursor);
    cancel->setFixedHeight(36);
    cancel->setStyleSheet(QStringLiteral(
        "QPushButton { background: #F0F0FF; color: %1; border: 1px solid #E1E2FA;"
        " border-radius: 8px; padding: 0 14px; font-size: 10px; font-weight: 600; }"
        "QPushButton:hover { background: #E8E9FF; }").arg(kNavy));
    auto *save = new QPushButton(existing ? QStringLiteral("Save changes")
                                          : QStringLiteral("Add season"), &dialog);
    save->setCursor(Qt::PointingHandCursor);
    save->setFixedHeight(36);
    save->setDefault(true);
    save->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; color: white; border: none; border-radius: 8px;"
        " padding: 0 14px; font-size: 10px; font-weight: 700; }"
        "QPushButton:hover { background: #0D0D6B; }").arg(kNavy));
    buttons->addWidget(cancel);
    buttons->addWidget(save);
    layout->addSpacing(8);
    layout->addLayout(buttons);

    QObject::connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(save, &QPushButton::clicked, &dialog, [&dialog, percentage]()
    {
        bool validPercentage = false;
        percentage->text().toDouble(&validPercentage);
        if (!percentage->hasAcceptableInput() || !validPercentage)
        {
            QMessageBox::warning(&dialog, QStringLiteral("Invalid discount"),
                QStringLiteral("Enter a discount between 0.01% and 100.00%."));
            percentage->setFocus();
            return;
        }
        dialog.accept();
    });

    if (dialog.exec() != QDialog::Accepted)
        return false;

    result.startDate = startDate->date();
    result.endDate = endDate->date();
    result.percentage = percentage->text().toDouble();
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
