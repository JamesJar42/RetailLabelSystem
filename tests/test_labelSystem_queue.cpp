#include <cassert>
#include <string>
#include <vector>

#include "labelSystem.h"
#include "test_common.h"

void run_labelsystem_queue_tests() {
    {
        labelSystem ls;
        std::string text = "hello world 123abc";
        ls.UpperCaseWords(text);
        assert(text == "Hello World 123abc");
    }

    {
        labelSystem ls;
        ls.dtb.clear();
        ls.dtb.add(product("P1", 1.0f, "c1", 1));
        ls.dtb.add(product("P2", 2.0f, "c2", 0));

        const int matched = ls.queueProducts(std::vector<std::string>{"c1", "missing", "c2"}, 3);
        assert(matched == 2);
        assert(ls.dtb.searchByBarcode("c1").getLabelQuantity() == 4);
        assert(ls.dtb.searchByBarcode("c2").getLabelQuantity() == 3);

        const int emptyMatched = ls.queueProducts(std::vector<std::string>{}, 4);
        assert(emptyMatched == 0);

        const int dupMatched = ls.queueProducts(std::vector<std::string>{"c1", "c1"}, 1);
        assert(dupMatched == 2);
        assert(ls.dtb.searchByBarcode("c1").getLabelQuantity() == 6);

        // Current behavior allows negative deltas (used by some UI actions).
        const int negMatched = ls.queueProducts(std::vector<std::string>{"c1"}, -2);
        assert(negMatched == 1);
        assert(ls.dtb.searchByBarcode("c1").getLabelQuantity() == 4);
    }

    {
        labelSystem ls;
        ls.dtb.clear();
        ls.dtb.add(product("P1", 1.0f, "c1", 0));
        ls.dtb.add(product("P2", 2.0f, "c2", 1));

        ls.addAllToQueue(2);
        assert(ls.dtb.searchByBarcode("c1").getLabelQuantity() == 2);
        assert(ls.dtb.searchByBarcode("c2").getLabelQuantity() == 3);

        ls.clearQueue();
        assert(ls.dtb.searchByBarcode("c1").getLabelQuantity() == 0);
        assert(ls.dtb.searchByBarcode("c2").getLabelQuantity() == 0);

        ls.dtb.searchByBarcode("c1").setLabelQuantity(7);
        ls.dtb.searchByBarcode("c2").setLabelQuantity(9);
        ls.clearAllFlags();
        assert(ls.dtb.searchByBarcode("c1").getLabelQuantity() == 0);
        assert(ls.dtb.searchByBarcode("c2").getLabelQuantity() == 0);
    }
}
