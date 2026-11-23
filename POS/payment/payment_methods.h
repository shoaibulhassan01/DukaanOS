#pragma once

#include "payment_method.h"

namespace Pos
{
class CashPayment final : public PaymentMethod
{
public:
    QString mode() const override { return QStringLiteral("cash"); }
    bool requiresMerchant() const override { return false; }
    bool validate(double amountPaid, double payable, const QString &merchant,
        QString &error) const override;
};

class CardPayment final : public PaymentMethod
{
public:
    QString mode() const override { return QStringLiteral("card"); }
    bool requiresMerchant() const override { return true; }
    bool validate(double amountPaid, double payable, const QString &merchant,
        QString &error) const override;
};

class WalletPayment final : public PaymentMethod
{
public:
    QString mode() const override { return QStringLiteral("wallet"); }
    bool requiresMerchant() const override { return true; }
    bool validate(double amountPaid, double payable, const QString &merchant,
        QString &error) const override;
};
}
