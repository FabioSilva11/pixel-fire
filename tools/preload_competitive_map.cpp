#include "world/competitive_source.h"
#include <iostream>
#include <sys/resource.h>
#include <cassert>
#include <chrono>
int main() {
    try {
        zz::CompetitiveMapSource source;
        source.transferToClient = true;
        if (!source.map.collisionResident())
            throw std::runtime_error("Canonical map not resident");
        const auto start = std::chrono::steady_clock::now();
        zz::World world;
        world.beginFinitePreload(1440, 128, 1440, &source);
        std::vector<bool> seen(8100);
        size_t full = 0, lod = 0, processed = 0;
        auto size = [](const zz::ChunkMesh &m) {
            return (m.opaque.size() + m.cutout.size() + m.water.size()) * sizeof(zz::VoxelVertex);
        };
        while (!world.finitePreloadReady()) {
            world.preloadFinite(64, 8);
            for (auto *c : world.chunks())
                if (c->meshed && !seen[c->listIndex]) {
                    seen[c->listIndex] = true;
                    ++processed;
                    full += size(c->mesh);
                    for (const auto &m : c->lodMesh)
                        lod += size(m);
                    // Match Android: exact mesh moves to GPU progressively while collider and visual proxies
                    // stay.
                    c->mesh = zz::ChunkMesh{};
                }
        }
        source.map.releaseExpandedForClient();
        assert(world.chunks().size() == 8100 && processed == 8100);
        const auto count = world.chunks().size();
        world.stream(1430, 1430, 4, zz::StreamBudget::bulk());
        assert(world.chunks().size() == count);
        for (const auto *c : world.chunks())
            assert(c->generated && c->blocks.size() == 32768);
        rusage usage{};
        getrusage(RUSAGE_SELF, &usage);
        std::cout << "{\"chunks\":" << count << ",\"fullMeshMiB\":" << double(full) / 1048576
                  << ",\"proxyMeshMiB\":" << double(lod) / 1048576
                  << ",\"peakResidentMiB\":" << double(usage.ru_maxrss) / 1024 << ",\"seconds\":"
                  << std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() << "}\n";
    } catch (const std::exception &e) {
        std::cerr << "Preload audit: " << e.what() << "\n";
        return 1;
    }
}
