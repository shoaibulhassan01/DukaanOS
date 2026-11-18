#include "payment_methods.h"

#include <cmath>

namespace Pos
{
namespace
{
constexpr double kCent = 0.005;   // money is compared to the nearest paisa

bool checkPayable(double payable, QString &error)
{
    if (!std::isfinite(payable) || payable <= 0)
    {
        error = QStringLiteral("The bill total must be greater than zero.");
        return false;
    }
    return true;
}

bool checkMerchant(const QString &merchant, const QString &label, QString &error)
{
    const QString name = merchant.trimmed();
    if (name.isEmpty())
    {
        error = QStringLiteral("Enter the %1 name.").arg(label);
        return false;
    }
    if (name.contains(QLatin1Char('#')))
    {
        error = QStringLiteral("The %1 name cannot contain #.").arg(label);
        return false;
    }
    return true;
}

bool checkExactAmount(double amountPaid, double payable, QString &error)
{
    if (!std::isfinite(amountPaid) || std::abs(amountPaid - payable) > kCent)
    {
        error = QStringLiteral("The amount paid must equal the bill total (PKR %1).")
            .arg(payable, 0, 'f', 2);
        return false;
    }
    return true;
}
}

bool CashPayment::validate(double amountPaid, double payable, const QString &,
    QString &error) const
{
    if (!checkPayable(payable, error))
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
    if (!checkPayable(payable, error) || !checkMerchant(merchant, QStringLiteral("bank"), error) ||
        !checkExactAmount(amountPaid, payable, error))
        return false;
    error.clear();
    return true;
}

bool WalletPayment::validate(double amountPaid, double payable, const QString &merchant,
    QString &error) const
{
    if (!checkPayable(payable, error) || !checkMerchant(merchant, QStringLiteral("wallet"), error) ||
        !checkExactAmount(amountPaid, payable, error))
        return false;
    error.clear();
    return true;
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
    return { QStringLiteral("Cash"), QStringLiteral("Card"), QStringLiteral("Wallet") };
}
}
