#include "payment_methods.h"

#include <cmath>
#include <memory>

namespace Pos
{
namespace
{
constexpr double kCent = 0.005;

bool validPayable(double payable, QString &error)
{
    if (!std::isfinite(payable) || payable <= 0)
    {
        error = QStringLiteral("The bill total must be greater than zero.");
        return false;
    }
    return true;
}

bool validMerchant(const QString &merchant, const QString &label, QString &error)
{
    if (merchant.trimmed().isEmpty() || merchant.contains(QLatin1Char('#')))
    {
        error = QStringLiteral("Enter a valid %1 name.").arg(label);
        return false;
    }
    return true;
}

bool exactAmount(double received, double payable, QString &error)
{
    if (!std::isfinite(received) || std::abs(received - payable) > kCent)
    {
        error = QStringLiteral("Card and wallet payments must equal the bill total (PKR %1).")
            .arg(payable, 0, 'f', 2);
        return false;
    }
    return true;
}
}

bool CashPayment::validate(double amountPaid, double payable, const QString &,
    QString &error) const
{
    if (!validPayable(payable, error))
        return false;
    if (!std::isfinite(amountPaid) || amountPaid + kCent < payable)
    {
        error = QStringLiteral("Cash received is less than the bill total (PKR %1).")
            .arg(payable, 0, 'f', 2);
        return false;
    }
    error.clear();
    return true;
}

bool CardPayment::validate(double amountPaid, double payable, const QString &merchant,
    QString &error) const
{
    return validPayable(payable, error)
        && validMerchant(merchant, QStringLiteral("bank"), error)
        && exactAmount(amountPaid, payable, error);
}

bool WalletPayment::validate(double amountPaid, double payable, const QString &merchant,
    QString &error) const
{
    return validPayable(payable, error)
        && validMerchant(merchant, QStringLiteral("wallet"), error)
        && exactAmount(amountPaid, payable, error);
}

std::unique_ptr<PaymentMethod> createPaymentMethod(const QString &mode)
{
    const QString key = mode.trimmed().toLower();
    if (key == QStringLiteral("cash"))
        return std::make_unique<CashPayment>();
    if (key == QStringLiteral("card"))
        return std::make_unique<CardPayment>();
    if (key == QStringLiteral("wallet"))
        return std::make_unique<WalletPayment>();
    return nullptr;
}

QStringList paymentModes()
{
    return {QStringLiteral("Cash"), QStringLiteral("Card"), QStringLiteral("Wallet")};
}
}
