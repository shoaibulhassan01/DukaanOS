#pragma once

#include "payment_method.h"

namespace Pos
{
// Cash: the amount received must cover the bill; the change is given back.
class CashPayment : public PaymentMethod
{
public:
    QString mode() const override { return QStringLiteral("cash"); }
    bool requiresMerchant() const override { return false; }
    bool validate(double amountPaid, double payable, const QString &merchant,
        QString &error) const override;
};

// Card: the bank is required and the exact bill amount is charged.
class CardPayment : public PaymentMethod
{
public:
    QString mode() const override { return QStringLiteral("card"); }
    bool requiresMerchant() const override { return true; }
    bool validate(double amountPaid, double payable, const QString &merchant,
        QString &error) const override;
};

// Wallet: the wallet provider is required and the exact bill amount is paid.
class WalletPayment : public PaymentMethod
{
public:
    QString mode() const override { return QStringLiteral("wallet"); }
    bool requiresMerchant() const override { return true; }
    bool validate(double amountPaid, double payable, const QString &merchant,
        QString &error) const override;
};
}
