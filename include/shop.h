#pragma once
#include "product.h"
#include<vector>

class shop
{

    std::vector<product> database;

public:

    std::vector<product> &listProduct();

    void add(product pd);

    product& search(int choice);

};
