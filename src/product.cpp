//
// Created by Joshua Lyttle on 13/03/2024.
//
#include "../include/product.h"
#include <string>

product::product(std::string description, float price, std::string size, std::string barcode, bool labelFlag)
        : description(description), price(price), size(size),barcode(barcode), labelFlag(labelFlag) {}

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

std::string product::getBarcode() const {
    return barcode;
}

bool product::getLabelFlag() const
{
    return labelFlag;
}

std::string product::getSize() const
{
    return size;
}

void product::setDescription(const std::string &dcr)
{
    description = dcr;
}

void product::setPrice(float prc)
{
    price = prc;
}

void product::setBarcode(const std::string &bcd)
{
    barcode = bcd;
}

void product::setLabelFlag(bool lblF)
{
    labelFlag = lblF;
}

void product::setSize(const std::string &sz)
{
    size = sz;
}

std::string product::toString()
{
    std::string p = std::to_string(price);
    std::string rp = p.substr(0, p.find(".")+3);
    return "Name: " + description + " , Price: " + rp + ", Size: " + size + " , Barcode: " + barcode + " , Flag: "
    + std::to_string(labelFlag) + " End";
}

bool product::operator==(const product &p) const {

    if(this->description == p.description && this->price == p.price && this->size == p.size && this->barcode == p.barcode)
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



