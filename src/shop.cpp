#include "../include\/shop.h"
#include <vector>

void shop::add(product pd)
{

    database.push_back(pd);

}

std::vector<product>& shop::listProduct()
{
    return database;
}

product& shop::search(int choice)
{
    return  database.at(choice-1);;
}


