#pragma once

#include <QString>
#include <QStringList>
#include <memory>

namespace Pos
{
class PaymentMethod
{
public:
    virtual ~PaymentMethod() = default;
    virtual QString mode() const = 0;
    virtual bool requiresMerchant() const = 0;
    virtual bool validate(double amountPaid, double payable,
        const QString &merchant, QString &error) const = 0;
};

std::unique_ptr<PaymentMethod> createPaymentMethod(const QString &mode);
QStringList paymentModes();
}
