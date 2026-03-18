#pragma once
#include <iostream>

class product
{
    std::string description;
    float price;
    float originalPrice;
    std::string barcode;
    int labelQuantity;

public:
    product(std::string description, float price, std::string barcode, int labelQuantity, float originalPrice = 0.0f);

    ~product();

    void setDescription(const std::string &dcr);
    void setPrice(float prc);
    void setOriginalPrice(float prc);
    void setBarcode(const std::string &bcd);

    void setLabelQuantity(int qty);

    std::string getDescription();
    float getPrice() const;
    float getOriginalPrice() const;
    std::string getBarcode() const;
    int getLabelQuantity() const;

    std::string getName() const;

    bool operator==(const product& p) const ;

    std::string toString();

};
