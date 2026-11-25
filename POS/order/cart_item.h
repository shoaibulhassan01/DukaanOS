#pragma once

#include "../product/product.h"

namespace Pos
{
struct CartItem
{
    Product product;
    int quantity = 0;
};

struct CartTotals
{
    double withoutTax = 0;
    double tax = 0;
    double discount = 0;
    double seasonalDiscount = 0;
    double specialDiscount = 0;
    double payable = 0;
};
}
