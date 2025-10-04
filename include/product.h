#pragma once
#include <iostream>

class product
{
    std::string description;
    float price;
    std::string barcode;
    bool labelFlag;
    std::string size;

public:
    product(std::string description, float price, std::string size, std::string barcode, bool labelFlag);

    ~product();

    void setDescription(const std::string &dcr);
    void setPrice(float prc);
    void setBarcode(const std::string &bcd);
    void setLabelFlag(bool lblF);
    void setSize(const std::string &sz);

    std::string getDescription();
    float getPrice() const;
    std::string getBarcode() const;
    bool getLabelFlag() const;
    std::string getSize() const;
    std::string getName() const;

    bool operator==(const product& p) const ;

    std::string toString();

};
