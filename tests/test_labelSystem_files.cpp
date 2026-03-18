#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

#include "labelSystem.h"
#include "test_common.h"

namespace {
std::string tempPath(const std::string &name) {
    const std::filesystem::path path = std::filesystem::temp_directory_path() / ("RetailLabelSystem_" + name);
    return path.string();
}

void writeTextFile(const std::string &path, const std::string &contents) {
    std::ofstream out(path, std::ios::binary);
    out << contents;
    out.close();
}
}

void run_labelsystem_file_tests() {
    {
        labelSystem ls;
        labelConfig cfg{};
        cfg.TL = 10;
        cfg.TS = 11;
        cfg.PS = 12;
        cfg.TX = 13;
        cfg.TY = 14;
        cfg.PX = 15;
        cfg.PY = 16;
        cfg.STX = 17;
        cfg.STY = 18;
        cfg.XO = 19;

        ls.setLabelConfig(cfg);
        const labelConfig got = ls.getLabelConfig();
        assert(got.TL == 10);
        assert(got.TS == 11);
        assert(got.PS == 12);
        assert(got.XO == 19);
    }

    {
        labelSystem ls;
        labelConfig cfg{};
        cfg.TL = 1;
        cfg.TS = 2;
        cfg.PS = 3;
        cfg.TX = 4;
        cfg.TY = 5;
        cfg.PX = 6;
        cfg.PY = 7;
        cfg.STX = 8;
        cfg.STY = 9;
        cfg.XO = 10;
        ls.setLabelConfig(cfg);

        const std::string outPath = tempPath("config.txt");
        assert(ls.saveConfig(outPath));

        std::ifstream fin(outPath);
        assert(fin.is_open());
        const std::string contents((std::istreambuf_iterator<char>(fin)), std::istreambuf_iterator<char>());
        fin.close();

        assert(contents.find("labels 1 2 3 4 5 6 7 8 9 10") != std::string::npos);
        std::filesystem::remove(outPath);
    }

    {
        labelSystem ls;
        const std::string badPath = tempPath("missing_folder/config.txt");
        const bool ok = ls.saveConfig(badPath);
        assert(!ok);
    }

    {
        writeTextFile("labels.jpg", "x");
        writeTextFile("labels(1).jpg", "x");
        writeTextFile("labels(2).jpg", "x");

        labelSystem ls;
        const bool ok = ls.deletePagesWithRetry(10, 5, 10);
        assert(ok);
        assert(!std::filesystem::exists("labels.jpg"));
        assert(!std::filesystem::exists("labels(1).jpg"));
        assert(!std::filesystem::exists("labels(2).jpg"));
    }

    {
        labelSystem ls;
        // Should be treated as success if files are already absent.
        const bool ok = ls.deletePagesWithRetry(1, 1, 1);
        assert(ok);
    }

    {
        const std::string cfgPath = tempPath("label_config_load.txt");
        writeTextFile(cfgPath, "labels 21 22 23 24 25 26 27 28 29 30\n");

        labelSystem ls(cfgPath);
        const labelConfig got = ls.getLabelConfig();
        assert(got.TL == 21);
        assert(got.TS == 22);
        assert(got.PS == 23);
        assert(got.TX == 24);
        assert(got.TY == 25);
        assert(got.PX == 26);
        assert(got.PY == 27);
        assert(got.STX == 28);
        assert(got.STY == 29);
        assert(got.XO == 30);

        std::filesystem::remove(cfgPath);
    }
}
