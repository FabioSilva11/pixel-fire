#include "world/competitive_map.h"
#include <fstream>
#include <iostream>
#include <iomanip>
int main(int argc, char **argv) {
    if (argc < 2 || argc > 3) {
        std::cerr << "usage: bake_map output.zzmap [manifest.json]\n";
        return 2;
    }
    auto map = zz::br::CompetitiveMap::bakeMission([](int done, int total) {
        if (done % 900 == 0)
            std::cout << "Mission chunks baked " << done << "/" << total << std::endl;
    });
    std::ofstream out(argv[1], std::ios::binary);
    map.save(out);
    if (!out)
        return 1;
    if (argc == 3) {
        std::ofstream json(argv[2]);
        json << "{\n  \"source\": \"IslandGenerator/Horizonte (main mission)\", \"preload\": \"full\", "
                "\"originX\": "
             << zz::br::kMissionOrigin << ", \"originZ\": " << zz::br::kMissionOrigin
             << ",\n  \"version\": " << zz::br::kMapVersion
             << ", \"sizeMeters\": 1440, \"seed\": " << zz::br::kMapSeed << ", \"hash\": " << map.hash()
             << ",\n  \"regions\": [\n";
        bool first = true;
        for (const auto &r : map.regions()) {
            if (!first)
                json << ",\n";
            first = false;
            json << "    {\"name\": " << std::quoted(r.name) << ", \"x\": " << r.x << ", \"z\": " << r.z
                 << ", \"elevation\": " << r.elevation << ", \"style\": " << r.style << "}";
        }
        json << "\n  ],\n  \"buildings\": [\n";
        first = true;
        for (const auto &b : map.buildings()) {
            if (!first)
                json << ",\n";
            first = false;
            json << "    {\"x\": " << b.x << ", \"z\": " << b.z << ", \"rotation\": " << b.rotation
                 << ", \"width\": " << b.width << ", \"depth\": " << b.depth << ", \"height\": " << b.height
                 << ", \"floor\": " << b.floor << ", \"style\": " << b.style << "}";
        }
        json << "\n  ]\n}\n";
        if (!json)
            return 1;
    }
    std::cout << "mapVersion=" << zz::br::kMapVersion << " hash=" << map.hash()
              << " regions=" << map.regions().size() << " buildings=" << map.buildings().size() << "\n";
}
