#include <iostream>
#include <stdexcept>

#include "test_common.h"

int main() {
    try {
        run_product_tests();
    } catch (const std::exception &ex) {
        std::cerr << "product_tests failure: " << ex.what() << std::endl;
        return 1;
    }
    std::cout << "product_tests passed" << std::endl;
    return 0;
}
