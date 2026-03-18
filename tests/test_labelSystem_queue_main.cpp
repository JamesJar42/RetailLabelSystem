#include <iostream>
#include <stdexcept>

#include "test_common.h"

int main() {
    try {
        run_labelsystem_queue_tests();
    } catch (const std::exception &ex) {
        std::cerr << "labelsystem_queue_tests failure: " << ex.what() << std::endl;
        return 1;
    }
    std::cout << "labelsystem_queue_tests passed" << std::endl;
    return 0;
}
