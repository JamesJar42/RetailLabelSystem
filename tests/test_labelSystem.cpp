#include <iostream>
#include <cassert>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>

#include "labelSystem.h"

// Helper to create a temporary file with given name and optional content
static void touch_file(const std::string &name, const std::string &contents = "") {
    std::ofstream out(name, std::ios::binary);
    out << contents;
    out.close();
}

int main() {
    // Test 1: UpperCaseWords
    {
        labelSystem ls;
        std::string s = "hello world 123abc";
        ls.UpperCaseWords(s);
        assert(s.find("Hello") != std::string::npos);
        assert(s.find("World") != std::string::npos);
        assert(s.find("123abc") != std::string::npos);
    }

    // Test 2: product basic setters/getters and equality
    {
        product p("apple", 1.99f, "500g", "0001", true);
        assert(p.getName() == "apple");
        assert(p.getPrice() == 1.99f);
        assert(p.getBarcode() == "0001");
        p.setDescription("banana");
        p.setPrice(2.50f);
        p.setBarcode("0002");
        p.setSize("1kg");
        p.setLabelFlag(false);
        assert(p.getName() == "banana");
        assert(p.getPrice() == 2.50f);
        assert(p.getBarcode() == "0002");
        assert(p.getSize() == "1kg");
        assert(p.getLabelFlag() == false);
    }

    // Test 3: shop add/list/search/remove/clear
    {
        shop s;
        s.clear();
        product a("A", 1.0f, "s", "b1", true);
        product b("B", 2.0f, "m", "b2", false);
        s.add(a);
        s.add(b);
        auto &list = s.listProduct();
        assert(list.size() == 2);
        // searchByBarcode
        product &found = s.searchByBarcode("b2");
        assert(found.getBarcode() == "b2");
        // removeByBarcode
        s.removeByBarcode("b1");
        assert(s.listProduct().size() == 1);
        // clear
        s.clear();
        assert(s.listProduct().empty());
    }

    // Test 4: labelSystem get/set config roundtrip
    {
        labelSystem ls;
        labelConfig cfg{};
        cfg.TL = 10; cfg.TS = 11; cfg.PS = 12; cfg.TX = 13; cfg.TY = 14; cfg.PX = 15; cfg.PY = 16; cfg.STX = 17; cfg.STY = 18; cfg.XO = 19;
        ls.setLabelConfig(cfg);
        labelConfig got = ls.getLabelConfig();
        assert(got.TL == 10);
        assert(got.XO == 19);
    }

    // Test 5: saveConfig writes and contains expected token
    {
        labelSystem ls;
        labelConfig cfg{};
        cfg.TL = 1; cfg.TS = 2; cfg.PS = 3; cfg.TX = 4; cfg.TY = 5; cfg.PX = 6; cfg.PY = 7; cfg.STX = 8; cfg.STY = 9; cfg.XO = 10;
        ls.setLabelConfig(cfg);
        const std::string outPath = "test_Config.txt";
        bool ok = ls.saveConfig(outPath);
        assert(ok);
        std::ifstream fin(outPath);
        assert(fin.is_open());
        std::string contents((std::istreambuf_iterator<char>(fin)), std::istreambuf_iterator<char>());
        fin.close();
        assert(contents.find("labels") != std::string::npos);
        std::remove(outPath.c_str());
    }

    // Test 6: clearLabelFlags -> ensure flags cleared across shop
    {
        labelSystem ls;
        ls.dtb.clear();
        ls.dtb.add(product("P1", 1.0f, "s", "c1", true));
        ls.dtb.add(product("P2", 2.0f, "m", "c2", true));
        ls.clearLabelFlags();
        for (const auto &p : ls.dtb.listProduct()) {
            assert(p.getLabelFlag() == false);
        }
    }

    // Test 7: deletePagesWithRetry deletes files created in working dir
    {
        // Create the files the function tries to remove
        touch_file("labels.jpg", "x");
        touch_file("labels(1).jpg", "x");
        touch_file("labels(2).jpg", "x");

        labelSystem ls;
        // labelVector is private; create temp files and call deletion routine
        // The function will wait a bit internally; keep attempts small to run fast
        bool ok = ls.deletePagesWithRetry(10, 5, 10);
        assert(ok == true);
        // Ensure files are gone
        std::ifstream f1("labels.jpg");
        std::ifstream f2("labels(1).jpg");
        std::ifstream f3("labels(2).jpg");
        assert(!f1.is_open());
        assert(!f2.is_open());
        assert(!f3.is_open());
    }

    std::cout << "All labelSystem tests passed\n";
    return 0;
}

