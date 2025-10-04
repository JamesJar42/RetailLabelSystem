#include "../include\/shop.h"
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <QDebug>

void shop::add(product pd)
{

    database.push_back(pd);

}

std::vector<product>& shop::listProduct()
{
    qDebug() << "listProduct called. Database size:" << database.size();
    for (const auto& pd : database) {
        qDebug() << "Product:" << QString::fromStdString(pd.getName())
                 << QString::fromStdString(pd.getBarcode())
                 << pd.getPrice();
    }
    return database;
}

product& shop::search(int choice)
{
    return  database.at(choice-1);;
}

void shop::removeByBarcode(const std::string &barcode)
{
    database.erase(std::remove_if(database.begin(), database.end(), [&](const product &p) {
        return p.getBarcode() == barcode;
    }), database.end());
}

product& shop::searchByBarcode(const std::string &barcode)
{
    auto it = std::find_if(database.begin(), database.end(), [&](const product &p) {
        return p.getBarcode() == barcode;
    });

    if (it == database.end()) {
        throw std::runtime_error("Product with the given barcode not found.");
    }

    return *it;
}

void shop::clear()
{
    database.clear();
}


