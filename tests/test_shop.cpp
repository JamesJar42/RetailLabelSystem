#include <cassert>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include "shop.h"
#include "test_common.h"

namespace {
std::string tempPath(const std::string &name) {
    const std::filesystem::path path = std::filesystem::temp_directory_path() / ("RetailLabelSystem_" + name);
    return path.string();
}
}

void run_shop_tests() {
    {
        shop s;
        s.clear();
        s.add(product("A", 1.0f, "b1", 1));
        s.add(product("B", 2.0f, "b2", 0));

        assert(s.listProduct().size() == 2);
        assert(s.search(1).getBarcode() == "b1");
        assert(s.searchByBarcode("b2").getDescription() == "B");

        bool thrown = false;
        try {
            (void)s.search(0);
        } catch (const std::out_of_range &) {
            thrown = true;
        }
        assert(thrown);

        s.removeByBarcode("b1");
        assert(s.listProduct().size() == 1);

        thrown = false;
        try {
            (void)s.searchByBarcode("missing");
        } catch (const std::runtime_error &) {
            thrown = true;
        }
        assert(thrown);

        s.clear();
        assert(s.listProduct().empty());
    }

    {
        shop s;
        s.clear();
        s.add(product("Milk", 1.10f, "111", 2, 1.30f));
        s.add(product("Bread", 2.20f, "222", 0, 2.50f));

        const std::string csvPath = tempPath("roundtrip.csv");
        assert(s.saveToCSV(csvPath));

        shop loaded;
        assert(loaded.loadFromCSV(csvPath));
        assert(loaded.listProduct().size() == 2);
        assert(loaded.searchByBarcode("111").getDescription() == "Milk");
        assert(loaded.searchByBarcode("111").getLabelQuantity() == 2);

        std::filesystem::remove(csvPath);
    }

    {
        const std::string csvPath = tempPath("header_and_bool.csv");
        std::ofstream out(csvPath);
        out << "Barcode,Name,Price,OriginalPrice,LabelFlag\n";
        out << "333,Eggs,4.10,5.25,true\n";
        out << "444,Tea,1.20,1.20,0\n";
        out.close();

        shop loaded;
        CSVMapping map;
        map.hasHeader = false;
        map.barcodeCol = 0;
        map.nameCol = 1;
        map.priceCol = 2;
        map.originalPriceCol = 3;
        map.labelQuantityCol = 4;

        assert(loaded.loadFromCSV(csvPath, map));
        assert(loaded.listProduct().size() == 2);
        assert(loaded.searchByBarcode("333").getLabelQuantity() == 1);
        assert(loaded.searchByBarcode("444").getLabelQuantity() == 0);

        std::filesystem::remove(csvPath);
    }

    {
        shop s;
        const bool ok = s.validateCloverConnection("merchant", "", true);
        assert(!ok);
        assert(!s.getLastOAuthError().empty());
    }

    {
        shop s;
        const bool loaded = s.loadFromCSV(tempPath("does_not_exist.csv"));
        assert(!loaded);
    }

    {
        const std::string csvPath = tempPath("malformed_rows.csv");
        std::ofstream out(csvPath);
        out << "Barcode,Name,Price,OriginalPrice,LabelFlag\n";
        out << "555,Coffee,not_a_number,3.40,2\n";
        out << "666,,1.99,2.50,true\n";
        out << ",MissingBarcode,2.10,2.10,0\n";
        out.close();

        shop loaded;
        CSVMapping map;
        map.hasHeader = true;
        map.barcodeCol = 0;
        map.nameCol = 1;
        map.priceCol = 2;
        map.originalPriceCol = 3;
        map.labelQuantityCol = 4;

        assert(loaded.loadFromCSV(csvPath, map));
        // Row with missing barcode should be skipped, malformed numeric fields should degrade to defaults.
        assert(loaded.listProduct().size() == 2);
        assert(loaded.searchByBarcode("555").getPrice() == 0.0f);
        assert(loaded.searchByBarcode("666").getDescription().empty());

        std::filesystem::remove(csvPath);
    }

    {
        shop s;
        s.clear();
        s.add(product("First", 1.0f, "dup", 1));
        s.add(product("Second", 2.0f, "dup", 2));
        // Index keeps the first occurrence for duplicate barcodes.
        assert(s.searchByBarcode("dup").getDescription() == "First");
        s.removeByBarcode("dup");
        assert(s.listProduct().empty());
    }

    {
        shop s;
        s.add(product("A", 1.0f, "1", 0));
        const bool ok = s.saveToCSV(tempPath("missing_dir/out.csv"));
        assert(!ok);
    }
}
