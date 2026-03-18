#include <iostream>
#include <stdexcept>

#include "test_common.h"

int main() {
    try {
        run_shop_tests();
    } catch (const std::exception &ex) {
        std::cerr << "shop_tests failure: " << ex.what() << std::endl;
        return 1;
    }
    std::cout << "shop_tests passed" << std::endl;
    return 0;
}
