#include "cashier_accounts_page.h"

#include "../../POS/common/data_paths.h"
#include "../../POS/user/user_repository.h"

#include <QAbstractItemView>
#include <QDialog>
#include <QFrame>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace
{
class CashierAccountsPage : public QWidget
{
public:
    explicit CashierAccountsPage(QWidget *parent) : QWidget(parent)
    {
        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 18);

        auto *card = new QFrame(this);
        card->setObjectName(QStringLiteral("cashierAccountsCard"));
        card->setStyleSheet(QStringLiteral(
            "QFrame#cashierAccountsCard { background: white; border: 1px solid #E7E9F3;"
            " border-radius: 14px; }"));
        auto *layout = new QVBoxLayout(card);
        layout->setContentsMargins(20, 18, 20, 20);
        layout->setSpacing(10);

        auto *title = new QLabel(QStringLiteral("Create cashier account"), card);
        title->setStyleSheet(QStringLiteral(
            "color: #20233B; font-size: 15px; font-weight: 700; background: transparent;"));
        layout->addWidget(title);

        username_ = addField(layout, card, QStringLiteral("Username"));
        password_ = addField(layout, card, QStringLiteral("Password"));
        password_->setEchoMode(QLineEdit::Password);
        confirmPassword_ = addField(layout, card, QStringLiteral("Confirm password"));
        confirmPassword_->setEchoMode(QLineEdit::Password);

        auto *createButton = new QPushButton(QStringLiteral("Create account"), card);
        createButton->setObjectName(QStringLiteral("createCashierAccountButton"));
        createButton->setFixedHeight(40);
        createButton->setStyleSheet(QStringLiteral(
            "QPushButton { background: #111184; color: white; border: none;"
            " border-radius: 8px; padding: 0 14px; font-weight: 600; }"
            "QPushButton:hover { background: #0D0D6B; }"));
        layout->addWidget(createButton, 0, Qt::AlignLeft);

        status_ = new QLabel(card);
        status_->setWordWrap(true);
        status_->hide();
        layout->addWidget(status_);
        root->addWidget(card);

        auto *accountsCard = new QFrame(this);
        accountsCard->setObjectName(QStringLiteral("cashierAccountsListCard"));
        accountsCard->setStyleSheet(QStringLiteral(
            "QFrame#cashierAccountsListCard { background: white; border: 1px solid #E7E9F3;"
            " border-radius: 14px; }"));
        auto *accountsLayout = new QVBoxLayout(accountsCard);
        accountsLayout->setContentsMargins(20, 18, 20, 18);
        accountsLayout->setSpacing(10);

        auto *accountsTitle = new QLabel(QStringLiteral("Existing cashier accounts"), accountsCard);
        accountsTitle->setStyleSheet(QStringLiteral(
            "color: #20233B; font-size: 15px; font-weight: 700; background: transparent;"));
        accountsLayout->addWidget(accountsTitle);

        auto *accountsHint = new QLabel(
            QStringLiteral("Manage cashier usernames and passwords."),
            accountsCard);
        accountsHint->setWordWrap(true);
        accountsHint->setStyleSheet(QStringLiteral(
            "color: #777B91; font-size: 11px; background: transparent;"));
        accountsLayout->addWidget(accountsHint);

        accountsTable_ = new QTableWidget(accountsCard);
        accountsTable_->setObjectName(QStringLiteral("cashierAccountsTable"));
        accountsTable_->setColumnCount(2);
        accountsTable_->setHorizontalHeaderLabels({
            QStringLiteral("Cashier name"), QStringLiteral("Action")
        });
        accountsTable_->verticalHeader()->hide();
        accountsTable_->setSelectionMode(QAbstractItemView::NoSelection);
        accountsTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        accountsTable_->setShowGrid(false);
        accountsTable_->setMinimumHeight(120);
        accountsTable_->setMaximumHeight(340);
        accountsTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        accountsTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
        accountsTable_->setColumnWidth(1, 120);
        accountsTable_->setStyleSheet(QStringLiteral(
            "QTableWidget { background: white; color: #20233B; border: none; }"
            "QHeaderView::section { background: #F8F8FC; color: #55596F; border: none;"
            " border-bottom: 1px solid #E7E9F3; padding: 8px; font-size: 10px; font-weight: 700; }"
            "QTableWidget::item { padding: 8px; border-bottom: 1px solid #F0F1F6; }"));
        accountsLayout->addWidget(accountsTable_);

        accountsStatus_ = new QLabel(accountsCard);
        accountsStatus_->setWordWrap(true);
        accountsStatus_->hide();
        accountsLayout->addWidget(accountsStatus_);
        root->addWidget(accountsCard, 1);

        connect(createButton, &QPushButton::clicked,
            this, [this]() { createAccount(); });
        connect(confirmPassword_, &QLineEdit::returnPressed,
            this, [this]() { createAccount(); });
        refreshCashierList();
    }

private:
    QLineEdit *addField(QVBoxLayout *layout, QWidget *parent,
        const QString &labelText)
    {
        auto *label = new QLabel(labelText, parent);
        label->setStyleSheet(QStringLiteral(
            "color: #20233B; font-size: 11px; font-weight: 600; background: transparent;"));
        layout->addWidget(label);

        auto *field = new QLineEdit(parent);
        field->setPlaceholderText(QStringLiteral("Enter %1").arg(labelText.toLower()));
        field->setFixedHeight(38);
        field->setStyleSheet(QStringLiteral(
            "QLineEdit { background: #FAFAFD; color: #20233B; border: 1px solid #E7E9F3;"
            " border-radius: 8px; padding: 0 10px; }"
            "QLineEdit:focus { background: white; border: 1px solid #111184; }"));
        layout->addWidget(field);
        return field;
    }

    void showStatus(const QString &message, bool isError)
    {
        status_->setText(message);
        status_->setStyleSheet(QStringLiteral(
            "color: %1; background: transparent;").arg(
                isError ? QStringLiteral("#B42318") : QStringLiteral("#18764A")));
        status_->show();
    }

    void showAccountsStatus(const QString &message, bool isError)
    {
        accountsStatus_->setText(message);
        accountsStatus_->setStyleSheet(QStringLiteral(
            "color: %1; background: transparent;").arg(
                isError ? QStringLiteral("#B42318") : QStringLiteral("#18764A")));
        accountsStatus_->show();
    }

    QLineEdit *makeDialogField(QWidget *parent, const QString &placeholder,
        bool password = false)
    {
        auto *field = new QLineEdit(parent);
        field->setPlaceholderText(placeholder);
        field->setFixedHeight(40);
        field->setStyleSheet(QStringLiteral(
            "QLineEdit { background: #FAFAFD; color: #20233B; border: 1px solid #E7E9F3;"
            " border-radius: 8px; padding: 0 10px; font-size: 12px; }"
            "QLineEdit:focus { background: white; border: 1px solid #111184; }"));
        if (password)
            field->setEchoMode(QLineEdit::Password);
        return field;
    }

    void editAccount(const QString &currentUsername)
    {
        QDialog dialog(this);
        dialog.setObjectName(QStringLiteral("editCashierAccountDialog"));
        dialog.setWindowTitle(QStringLiteral("Edit cashier account"));
        dialog.setModal(true);
        dialog.setMinimumWidth(390);
        dialog.setStyleSheet(QStringLiteral(
            "QDialog { background: white; color: #20233B; }"));

        auto *layout = new QVBoxLayout(&dialog);
        layout->setContentsMargins(24, 22, 24, 20);
        layout->setSpacing(10);

        auto *title = new QLabel(QStringLiteral("Edit cashier account"), &dialog);
        title->setStyleSheet(QStringLiteral(
            "color: #20233B; font-size: 17px; font-weight: 700; background: transparent;"));
        layout->addWidget(title);

        auto *hint = new QLabel(
            QStringLiteral("Update the username or set a new password. Leave the password blank to keep it unchanged."),
            &dialog);
        hint->setWordWrap(true);
        hint->setStyleSheet(QStringLiteral(
            "color: #777B91; font-size: 11px; background: transparent;"));
        layout->addWidget(hint);

        auto *usernameLabel = new QLabel(QStringLiteral("Username"), &dialog);
        usernameLabel->setStyleSheet(QStringLiteral(
            "color: #20233B; font-size: 11px; font-weight: 600; background: transparent;"));
        layout->addWidget(usernameLabel);
        QLineEdit *usernameField = makeDialogField(
            &dialog, QStringLiteral("Enter username"));
        usernameField->setObjectName(QStringLiteral("cashierEditUsername"));
        usernameField->setText(currentUsername);
        layout->addWidget(usernameField);

        auto *passwordLabel = new QLabel(QStringLiteral("New password"), &dialog);
        passwordLabel->setStyleSheet(usernameLabel->styleSheet());
        layout->addWidget(passwordLabel);
        QLineEdit *passwordField = makeDialogField(
            &dialog, QStringLiteral("Leave blank to keep current password"), true);
        passwordField->setObjectName(QStringLiteral("cashierEditPassword"));
        layout->addWidget(passwordField);

        auto *status = new QLabel(&dialog);
        status->setObjectName(QStringLiteral("cashierEditStatus"));
        status->setWordWrap(true);
        status->hide();
        layout->addWidget(status);

        auto *actions = new QHBoxLayout;
        actions->addStretch(1);
        auto *cancelButton = new QPushButton(QStringLiteral("Cancel"), &dialog);
        cancelButton->setObjectName(QStringLiteral("cancelCashierEditButton"));
        cancelButton->setFixedHeight(38);
        cancelButton->setStyleSheet(QStringLiteral(
            "QPushButton { background: white; color: #55596F; border: 1px solid #E7E9F3;"
            " border-radius: 8px; padding: 0 16px; font-weight: 600; }"
            "QPushButton:hover { background: #F8F8FC; }"));
        auto *saveButton = new QPushButton(QStringLiteral("Save changes"), &dialog);
        saveButton->setObjectName(QStringLiteral("saveCashierEditButton"));
        saveButton->setFixedHeight(38);
        saveButton->setStyleSheet(QStringLiteral(
            "QPushButton { background: #111184; color: white; border: none;"
            " border-radius: 8px; padding: 0 16px; font-weight: 600; }"
            "QPushButton:hover { background: #0D0D6B; }"));
        actions->addWidget(cancelButton);
        actions->addWidget(saveButton);
        layout->addLayout(actions);

        connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);
        connect(saveButton, &QPushButton::clicked, &dialog, [&, this]() {
            QString error;
            if (!Pos::updateCashierAccount(Pos::DataPaths::users(),
                    currentUsername, usernameField->text(), passwordField->text(), error))
            {
                status->setText(error);
                status->setStyleSheet(QStringLiteral(
                    "color: #B42318; background: transparent;"));
                status->show();
                return;
            }
            dialog.accept();
        });

        if (dialog.exec() == QDialog::Accepted)
        {
            showAccountsStatus(QStringLiteral("Cashier account updated."), false);
            refreshCashierList();
        }
    }

    void refreshCashierList()
    {
        accountsTable_->setRowCount(0);
        QVector<Pos::User> users;
        QString error;
        if (!Pos::readUsers(Pos::DataPaths::users(), users, error))
        {
            showAccountsStatus(error, true);
            return;
        }

        int cashierCount = 0;
        for (const Pos::User &user : users)
        {
            if (user.role != QStringLiteral("cashier"))
                continue;

            ++cashierCount;
            const int row = accountsTable_->rowCount();
            accountsTable_->insertRow(row);
            auto *currentUsername = new QTableWidgetItem(user.username);
            currentUsername->setFlags(currentUsername->flags() & ~Qt::ItemIsEditable);
            accountsTable_->setItem(row, 0, currentUsername);

            auto *editButton = new QPushButton(QStringLiteral("Edit"), accountsTable_);
            editButton->setObjectName(QStringLiteral("editCashierAccountButton"));
            editButton->setCursor(Qt::PointingHandCursor);
            editButton->setFixedSize(78, 34);
            editButton->setStyleSheet(QStringLiteral(
                "QPushButton { background: #111184; color: white; border: none;"
                " border-radius: 7px; font-size: 11px; font-weight: 700; }"
                "QPushButton:hover { background: #0D0D6B; }"));
            accountsTable_->setCellWidget(row, 1, editButton);
            connect(editButton, &QPushButton::clicked, this,
                [this, currentUsername = user.username]() { editAccount(currentUsername); });
            accountsTable_->setRowHeight(row, 50);
        }
        if (cashierCount == 0)
        {
            accountsTable_->setRowCount(1);
            auto *empty = new QTableWidgetItem(QStringLiteral("No cashier accounts yet."));
            empty->setFlags(empty->flags() & ~Qt::ItemIsEditable);
            accountsTable_->setItem(0, 0, empty);
            accountsTable_->setSpan(0, 0, 1, 2);
        }
    }

    void createAccount()
    {
        if (password_->text() != confirmPassword_->text())
        {
            showStatus(QStringLiteral("Passwords do not match."), true);
            return;
        }

        QString error;
        if (!Pos::createCashierAccount(Pos::DataPaths::users(),
                username_->text(), password_->text(), error))
        {
            showStatus(error, true);
            return;
        }

        username_->clear();
        password_->clear();
        confirmPassword_->clear();
        showStatus(QStringLiteral("Cashier account created."), false);
        refreshCashierList();
    }

    QLineEdit *username_ = nullptr;
    QLineEdit *password_ = nullptr;
    QLineEdit *confirmPassword_ = nullptr;
    QLabel *status_ = nullptr;
    QTableWidget *accountsTable_ = nullptr;
    QLabel *accountsStatus_ = nullptr;
};
}

QWidget *UiPages::createCashierAccountsPage(QWidget *parent)
{
    return new CashierAccountsPage(parent);
}
