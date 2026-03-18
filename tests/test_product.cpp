#include <cassert>
#include <string>

#include "product.h"
#include "test_common.h"

void run_product_tests() {
    {
        product p("Apple", 1.99f, "0001", 5);
        assert(p.getDescription() == "Apple");
        assert(p.getName() == "Apple");
        assert(p.getPrice() == 1.99f);
        assert(p.getOriginalPrice() == 1.99f);
        assert(p.getBarcode() == "0001");
        assert(p.getLabelQuantity() == 5);
    }

    {
        product p("Pear", 2.49f, "0002", 2, 3.99f);
        assert(p.getOriginalPrice() == 3.99f);

        p.setDescription("Banana");
        p.setPrice(1.50f);
        p.setOriginalPrice(2.20f);
        p.setBarcode("0003");
        p.setLabelQuantity(8);

        assert(p.getDescription() == "Banana");
        assert(p.getPrice() == 1.50f);
        assert(p.getOriginalPrice() == 2.20f);
        assert(p.getBarcode() == "0003");
        assert(p.getLabelQuantity() == 8);
    }

    {
        product a("Name", 4.50f, "ABCD", 1);
        product b("Name", 4.50f, "ABCD", 99);
        product c("Different", 4.50f, "ABCD", 1);

        assert(a == b);
        assert(!(a == c));
    }

    {
        product p("Orange", 3.75f, "ZX1", 7);
        const std::string summary = p.toString();
        assert(summary.find("Name: Orange") != std::string::npos);
        assert(summary.find("Barcode: ZX1") != std::string::npos);
        assert(summary.find("Quantity: 7") != std::string::npos);
        assert(summary.find("Price:") != std::string::npos);
    }
}
