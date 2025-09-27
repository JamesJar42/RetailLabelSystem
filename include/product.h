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

    void setDescription(std::string &dcr);
    void setPrice(float &prc);
    void setBarcode(std::string &bcd);
    void setLabelFlag(bool lblF);
    void setSize(std::string &sz);

    std::string getDescription();
    float& getPrice();
    std::string& getBarcode();
    bool& getLabelFlag();
    std::string& getSize();

    bool operator==(const product& p) const ;

    std::string toString();

};
