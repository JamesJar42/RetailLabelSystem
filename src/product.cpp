//
// Created by Joshua Lyttle on 13/03/2024.
//
#include "../include/product.h"
#include <string>

product::product(std::string description, float price, std::string barcode, int labelQuantity, float originalPrice)
        : description(description), price(price), barcode(barcode), labelQuantity(labelQuantity), originalPrice(originalPrice) 
{
    if (this->originalPrice == 0.0f) this->originalPrice = price; 
}

product::~product()
{
}

std::string product::getDescription()
{
    return description;
}

float product::getPrice() const {
    return price;
}

float product::getOriginalPrice() const {
    return originalPrice;
}

std::string product::getBarcode() const {
    return barcode;
}

int product::getLabelQuantity() const
{
    return labelQuantity;
}

void product::setDescription(const std::string &dcr)
{
    description = dcr;
}

void product::setPrice(float prc)
{
    price = prc;
}

void product::setOriginalPrice(float prc)
{
    originalPrice = prc;
}

void product::setBarcode(const std::string &bcd)
{
    barcode = bcd;
}

void product::setLabelQuantity(int qty) {
    labelQuantity = qty;
}

std::string product::toString()
{
    std::string p = std::to_string(price);
    std::string rp = p.substr(0, p.find(".")+3);
    return "Name: " + description + " , Price: " + rp + " , Barcode: " + barcode + " , Quantity: "
    + std::to_string(labelQuantity) + " End";
}

bool product::operator==(const product &p) const {

    if(this->description == p.description && this->price == p.price && this->barcode == p.barcode)
    {
        return true;
    }
    else
    {
        return false;
    }
}

std::string product::getName() const
{
    return description;
}



