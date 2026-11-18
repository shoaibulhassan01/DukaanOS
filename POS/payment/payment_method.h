#pragma once

#include <QString>
#include <QStringList>
#include <memory>

namespace Pos
{
// One way of paying for an order. Open/Closed: a new payment mode is a new
// subclass plus one line in createPaymentMethod(); the checkout and the
// screens do not change.
class PaymentMethod
{
public:
    virtual ~PaymentMethod() = default;

    // "cash", "card" or "wallet", exactly as written in Order.txt.
    virtual QString mode() const = 0;

    // True if a merchant name (bank or wallet, e.g. HBL, JazzCash) is needed.
    virtual bool requiresMerchant() const = 0;

    // Checks the payment before anything is saved.
    virtual bool validate(double amountPaid, double payable,
        const QString &merchant, QString &error) const = 0;
};

// Returns the payment method for "cash", "card" or "wallet" (any letter
// case), or nullptr for an unknown mode.
std::unique_ptr<PaymentMethod> createPaymentMethod(const QString &mode);

// The modes offered on the New Order screen.
QStringList paymentModes();
}
