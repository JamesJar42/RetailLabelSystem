#include <iostream>
#include <stdexcept>

#include "test_common.h"

int main() {
    try {
        run_labelsystem_file_tests();
    } catch (const std::exception &ex) {
        std::cerr << "labelsystem_files_tests failure: " << ex.what() << std::endl;
        return 1;
    }
    std::cout << "labelsystem_files_tests passed" << std::endl;
    return 0;
}
