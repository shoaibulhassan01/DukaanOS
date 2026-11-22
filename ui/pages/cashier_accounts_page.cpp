#include "cashier_accounts_page.h"

#include "../../POS/common/data_paths.h"
#include "../../POS/user/user_repository.h"

#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
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
        root->addStretch(1);

        connect(createButton, &QPushButton::clicked,
            this, [this]() { createAccount(); });
        connect(confirmPassword_, &QLineEdit::returnPressed,
            this, [this]() { createAccount(); });
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
    }

    QLineEdit *username_ = nullptr;
    QLineEdit *password_ = nullptr;
    QLineEdit *confirmPassword_ = nullptr;
    QLabel *status_ = nullptr;
};
}

QWidget *UiPages::createCashierAccountsPage(QWidget *parent)
{
    return new CashierAccountsPage(parent);
}
