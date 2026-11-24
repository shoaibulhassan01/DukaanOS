#include "categories_page.h"

#include "../../POS/category/category_repository.h"
#include "../../POS/category/category_service.h"
#include "../../POS/common/data_paths.h"

#include <QDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace
{
const QString kNavy = QStringLiteral("#111184");
const QString kInk = QStringLiteral("#20233B");
const QString kMuted = QStringLiteral("#777B91");
const QString kLine = QStringLiteral("#E7E9F3");

QString display(QString field)
{
    return field.replace(QLatin1Char('_'), QLatin1Char(' '));
}

QString buttonStyle(bool primary)
{
    if (primary)
        return QStringLiteral(
            "QPushButton { background: %1; color: white; border: none; border-radius: 8px;"
            " padding: 0 14px; font-size: 10px; font-weight: 700; }"
            "QPushButton:hover { background: #0D0D6B; }").arg(kNavy);
    return QStringLiteral(
        "QPushButton { background: #F0F0FF; color: %1; border: 1px solid #E1E2FA;"
        " border-radius: 8px; padding: 0 14px; font-size: 10px; font-weight: 600; }"
        "QPushButton:hover { background: #E8E9FF; }"
        "QPushButton:disabled { color: #A9ABBD; background: #F6F6FA; border-color: %2; }")
        .arg(kNavy, kLine);
}

// Add / Edit form. Returns true if the category was saved; savedId gets
// its Id (used by the Add product form to select it).
bool showCategoryDialog(QWidget *parent, const Pos::Category *existing,
    const QString &categoryFile = Pos::DataPaths::categories(), QString *savedId = nullptr)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(existing ? QStringLiteral("Edit category") : QStringLiteral("Add category"));
    dialog.setMinimumWidth(380);
    dialog.setStyleSheet(QStringLiteral("QDialog { background: white; }"));
    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(22, 20, 22, 18);
    layout->setSpacing(8);

    auto *title = new QLabel(dialog.windowTitle(), &dialog);
    title->setStyleSheet(QStringLiteral("color: %1; font-size: 15px; font-weight: 700;").arg(kInk));
    layout->addWidget(title);
    auto *hint = new QLabel(QStringLiteral(
        "The Category Id (10 to 99) is the first two digits of the barcode of every product in this category."), &dialog);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color: %1; font-size: 10px;").arg(kMuted));
    layout->addWidget(hint);

    auto addField = [&](const QString &label, const QString &placeholder) {
        auto *caption = new QLabel(label, &dialog);
        caption->setStyleSheet(QStringLiteral("color: %1; font-size: 11px; font-weight: 600;").arg(kInk));
        layout->addWidget(caption);
        auto *field = new QLineEdit(&dialog);
        field->setPlaceholderText(placeholder);
        field->setFixedHeight(38);
        field->setStyleSheet(QStringLiteral(
            "QLineEdit { background: #FAFAFD; color: %1; border: 1px solid %2;"
            " border-radius: 8px; padding: 0 10px; }"
            "QLineEdit:focus { background: white; border: 1px solid %3; }"
            "QLineEdit:disabled { color: %4; background: #F6F6FA; }").arg(kInk, kLine, kNavy, kMuted));
        layout->addWidget(field);
        return field;
    };
    QLineEdit *id = addField(QStringLiteral("Category Id (10 to 99)"), QStringLiteral("e.g. 10"));
    id->setMaxLength(2);
    // Barcodes cannot start with 0, so the Id cannot either.
    id->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("([1-9]\\d?)?")), id));
    QLineEdit *name = addField(QStringLiteral("Category name"), QStringLiteral("e.g. Beverages"));
    QLineEdit *acronym = addField(QStringLiteral("Category acronym"), QStringLiteral("e.g. BEV"));
    if (existing)
    {
        id->setText(existing->id);
        id->setEnabled(false);
        id->setToolTip(QStringLiteral("The Id cannot be changed: it is the start of the products' barcodes."));
        name->setText(display(existing->name));
        acronym->setText(display(existing->acronym));
    }

    auto *error = new QLabel(&dialog);
    error->setWordWrap(true);
    error->setStyleSheet(QStringLiteral("color: #B42318; font-size: 10px;"));
    error->hide();
    layout->addWidget(error);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch(1);
    auto *cancel = new QPushButton(QStringLiteral("Cancel"), &dialog);
    cancel->setFixedHeight(36);
    cancel->setStyleSheet(buttonStyle(false));
    auto *save = new QPushButton(existing ? QStringLiteral("Save changes") : QStringLiteral("Add category"), &dialog);
    save->setFixedHeight(36);
    save->setDefault(true);
    save->setStyleSheet(buttonStyle(true));
    buttons->addWidget(cancel);
    buttons->addWidget(save);
    layout->addSpacing(6);
    layout->addLayout(buttons);

    QObject::connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(save, &QPushButton::clicked, &dialog, [&]() {
        const Pos::Category category{id->text(), name->text(), acronym->text()};
        QString message;
        const bool saved = existing
            ? Pos::updateCategory(categoryFile, category, message)
            : Pos::addCategory(categoryFile, category, message);
        if (!saved)
        {
            error->setText(message);
            error->show();
            return;
        }
        if (savedId)
            *savedId = id->text().trimmed();
        dialog.accept();
    });
    (existing ? name : id)->setFocus();
    return dialog.exec() == QDialog::Accepted;
}

class CategoriesPage : public QWidget
{
public:
    explicit CategoriesPage(QWidget *parent) : QWidget(parent)
    {
        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 18);
        root->setSpacing(12);

        auto *card = new QFrame(this);
        card->setObjectName(QStringLiteral("categoriesCard"));
        card->setStyleSheet(QStringLiteral(
            "QFrame#categoriesCard { background: white; border: 1px solid %1; border-radius: 14px; }")
            .arg(kLine));
        auto *layout = new QVBoxLayout(card);
        layout->setContentsMargins(18, 18, 18, 16);
        layout->setSpacing(13);

        auto *heading = new QHBoxLayout;
        auto *title = new QLabel(QStringLiteral("Product categories"), card);
        title->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 15px; font-weight: 700; background: transparent;").arg(kInk));
        heading->addWidget(title);
        heading->addStretch(1);
        countLabel_ = new QLabel(card);
        countLabel_->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 10px; background: transparent;").arg(kMuted));
        heading->addWidget(countLabel_);
        heading->addSpacing(10);

        editButton_ = new QPushButton(QStringLiteral("Edit selected"), card);
        editButton_->setCursor(Qt::PointingHandCursor);
        editButton_->setFixedHeight(36);
        editButton_->setEnabled(false);
        editButton_->setToolTip(QStringLiteral("Change the name or acronym (or double-click a row)"));
        editButton_->setStyleSheet(buttonStyle(false));
        heading->addWidget(editButton_);

        deleteButton_ = new QPushButton(QStringLiteral("Delete"), card);
        deleteButton_->setCursor(Qt::PointingHandCursor);
        deleteButton_->setFixedHeight(36);
        deleteButton_->setEnabled(false);
        deleteButton_->setToolTip(QStringLiteral("Delete a category that no product uses"));
        deleteButton_->setStyleSheet(QStringLiteral(
            "QPushButton { background: #FEF3F2; color: #B42318; border: 1px solid #FECDCA;"
            " border-radius: 8px; padding: 0 14px; font-size: 10px; font-weight: 600; }"
            "QPushButton:hover { background: #FEE4E2; }"
            "QPushButton:disabled { color: #A9ABBD; background: #F6F6FA; border-color: %1; }")
            .arg(kLine));
        heading->addWidget(deleteButton_);

        auto *addButton = new QPushButton(QStringLiteral("+ Add category"), card);
        addButton->setCursor(Qt::PointingHandCursor);
        addButton->setFixedHeight(36);
        addButton->setStyleSheet(buttonStyle(true));
        heading->addWidget(addButton);
        layout->addLayout(heading);

        search_ = new QLineEdit(card);
        search_->setPlaceholderText(QStringLiteral("Search by name, ID or acronym"));
        search_->setFixedHeight(40);
        search_->setStyleSheet(QStringLiteral(
            "QLineEdit { background: #FAFAFD; color: %1; border: 1px solid %2;"
            " border-radius: 8px; padding: 0 12px; font-size: 11px; }"
            "QLineEdit:focus { background: white; border: 1px solid %3; }")
            .arg(kInk, kLine, kNavy));
        layout->addWidget(search_);

        table_ = new QTableWidget(card);
        table_->setColumnCount(4);
        table_->setHorizontalHeaderLabels({QStringLiteral("CATEGORY ID"), QStringLiteral("NAME"),
            QStringLiteral("ACRONYM"), QStringLiteral("PRODUCTS")});
        table_->verticalHeader()->hide();
        table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
        table_->horizontalHeader()->setFixedHeight(40);
        table_->setShowGrid(false);
        table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table_->setSelectionBehavior(QAbstractItemView::SelectRows);
        table_->setSelectionMode(QAbstractItemView::SingleSelection);
        table_->setStyleSheet(QStringLiteral(
            "QTableWidget { background: white; border: none; color: %1; outline: 0; }"
            "QTableWidget::item { padding: 5px 8px; border-bottom: 1px solid #F0F1F6; }"
            "QTableWidget::item:selected { background: #F0F0FF; color: %2; }"
            "QHeaderView::section { background: #F8F8FC; color: %3; border: none;"
            " border-bottom: 1px solid %4; padding: 0 8px; font-size: 9px; font-weight: 700; }")
            .arg(kInk, kNavy, kMuted, kLine));
        layout->addWidget(table_, 1);

        emptyLabel_ = new QLabel(QStringLiteral(
            "No categories yet. Add a category before adding products:\n"
            "its 2-digit Id becomes the first two digits of their barcodes."), card);
        emptyLabel_->setAlignment(Qt::AlignCenter);
        emptyLabel_->setWordWrap(true);
        emptyLabel_->setMinimumHeight(160);
        emptyLabel_->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 12px; background: #FAFAFD;"
            " border: 1px dashed %2; border-radius: 12px; padding: 18px;").arg(kMuted, kLine));
        layout->addWidget(emptyLabel_);
        root->addWidget(card, 1);

        note_ = new QLabel(this);
        note_->setWordWrap(true);
        root->addWidget(note_);

        connect(search_, &QLineEdit::textChanged, this, [this]() { showRows(); });
        connect(table_, &QTableWidget::itemSelectionChanged, this, [this]() {
            editButton_->setEnabled(selectedRow() >= 0);
            deleteButton_->setEnabled(selectedRow() >= 0);
        });
        connect(table_, &QTableWidget::cellDoubleClicked, this, [this](int, int) { editSelected(); });
        connect(editButton_, &QPushButton::clicked, this, [this]() { editSelected(); });
        connect(deleteButton_, &QPushButton::clicked, this, [this]() { deleteSelected(); });
        connect(addButton, &QPushButton::clicked, this, [this]() {
            if (showCategoryDialog(this, nullptr))
            {
                load();
                showNote(QStringLiteral("Category added to PCategory.txt."), false);
            }
        });
        load();
    }

private:
    void load()
    {
        QString error;
        if (!Pos::readCategories(Pos::DataPaths::categories(), categories_, error))
        {
            categories_.clear();
            visible_.clear();
            table_->hide();
            emptyLabel_->setText(QStringLiteral("Categories could not be loaded."));
            emptyLabel_->show();
            showNote(error, true);
            return;
        }

        productCounts_.clear();
        for (const Pos::Category &category : categories_)
        {
            QStringList products;
            QString productError;
            Pos::productsInCategory(Pos::DataPaths::products(), category.id, products, productError);
            productCounts_.insert(category.id, products.size());
        }
        showRows();
        showNote(QStringLiteral("Categories are saved in Data/PCategory.txt."), false, true);
    }

    // The categories that match the search box (search rules live in
    // category_service, not in the UI).
    void showRows()
    {
        visible_ = Pos::searchCategories(categories_, search_->text());
        table_->clearSelection();
        table_->setRowCount(visible_.size());
        for (int row = 0; row < visible_.size(); ++row)
        {
            const Pos::Category &category = visible_.at(row);
            setCell(row, 0, category.id);
            setCell(row, 1, display(category.name));
            setCell(row, 2, display(category.acronym));
            setCell(row, 3, QString::number(productCounts_.value(category.id)));
            table_->setRowHeight(row, 44);
        }
        table_->setVisible(!visible_.isEmpty());
        emptyLabel_->setVisible(visible_.isEmpty());
        emptyLabel_->setText(categories_.isEmpty()
            ? QStringLiteral("No categories yet. Add a category before adding products:\n"
                  "its 2-digit Id becomes the first two digits of their barcodes.")
            : QStringLiteral("No categories match your search."));
        const QString noun = categories_.size() == 1 ? QStringLiteral("category") : QStringLiteral("categories");
        countLabel_->setText(search_->text().trimmed().isEmpty()
            ? QStringLiteral("%1 %2").arg(categories_.size()).arg(noun)
            : QStringLiteral("Showing %1 of %2 %3").arg(visible_.size()).arg(categories_.size()).arg(noun));
        editButton_->setEnabled(false);
        deleteButton_->setEnabled(false);
    }

    int selectedRow() const
    {
        const QModelIndexList rows = table_->selectionModel()
            ? table_->selectionModel()->selectedRows() : QModelIndexList();
        if (rows.isEmpty() || rows.first().row() >= visible_.size())
            return -1;
        return rows.first().row();
    }

    void editSelected()
    {
        const int row = selectedRow();
        if (row < 0)
            return;
        const Pos::Category category = visible_.at(row);
        if (showCategoryDialog(this, &category))
        {
            load();
            showNote(QStringLiteral("Category %1 updated in PCategory.txt.").arg(category.id), false);
        }
    }

    void deleteSelected()
    {
        const int row = selectedRow();
        if (row < 0)
            return;
        const Pos::Category category = visible_.at(row);
        const auto answer = QMessageBox::question(this, QStringLiteral("Delete category"),
            QStringLiteral("Delete category %1 (%2) from PCategory.txt?")
                .arg(category.id, display(category.name)),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
        QString error;
        if (!Pos::deleteCategory(Pos::DataPaths::categories(), Pos::DataPaths::products(),
                category.id, error))
        {
            QMessageBox::warning(this, QStringLiteral("Cannot delete category"), error);
            return;
        }
        load();
        showNote(QStringLiteral("Category %1 deleted from PCategory.txt.").arg(category.id), false);
    }

    void showNote(const QString &text, bool isError, bool muted = false)
    {
        const QString color = isError ? QStringLiteral("#B42318")
            : (muted ? kMuted : QStringLiteral("#18764A"));
        note_->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 10px; font-weight: %2; background: transparent;")
            .arg(color, muted ? QStringLiteral("400") : QStringLiteral("600")));
        note_->setText(text);
    }

    void setCell(int row, int column, const QString &text)
    {
        auto *cell = new QTableWidgetItem(text);
        cell->setFlags(cell->flags() & ~Qt::ItemIsEditable);
        table_->setItem(row, column, cell);
    }

    QVector<Pos::Category> categories_;   // everything in PCategory.txt
    QVector<Pos::Category> visible_;      // the rows shown (search applied)
    QHash<QString, int> productCounts_;   // Category Id -> products using it
    QLineEdit *search_ = nullptr;
    QTableWidget *table_ = nullptr;
    QLabel *emptyLabel_ = nullptr;
    QLabel *countLabel_ = nullptr;
    QLabel *note_ = nullptr;
    QPushButton *editButton_ = nullptr;
    QPushButton *deleteButton_ = nullptr;
};
}

QWidget *UiPages::createCategoriesPage(QWidget *parent)
{
    return new CategoriesPage(parent);
}

bool UiPages::showAddCategoryDialog(QWidget *parent, const QString &categoryFile,
    QString *createdId)
{
    return showCategoryDialog(parent, nullptr, categoryFile, createdId);
}
