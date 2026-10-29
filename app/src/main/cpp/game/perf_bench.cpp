#include "perf_bench.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include "../core/perf.h"
#include "../world/island.h"
#include "../world/terrain.h"
#include "../world/world.h"

namespace zz::perf {

namespace {

// ----------------------------------------------------------------------------- fontes falsas (autotestes)

class FakeSource : public ChunkSource {
public:
    void generate(Chunk& chunk, int height) override {
        for (int y = 0; y < 10 && y < height; ++y)
            for (int i = 0; i < kChunkSize * kChunkSize; ++i) chunk.blocks[(static_cast<size_t>(y) << 8) | static_cast<size_t>(i)] = B_STONE;
        for (int16_t& g : chunk.ground) g = 10;
        spin();
    }
    virtual void spin() {}
};

// Espera ocupada de ~1,5 ms por chunk: custo previsível para testar o orçamento de tempo.
class SlowSource : public FakeSource {
public:
    void spin() override {
        const double t0 = nowMs();
        while (nowMs() - t0 < 1.5) {
        }
    }
};

struct TestLog {
    int failed = 0;
    void report(const char* name, bool ok) {
        ZZ_PERF_LOG("selftest %s %s", name, ok ? "PASS" : "FAIL");
        if (!ok) ++failed;
    }
};

constexpr int kFakeWorld = 512;
constexpr float kCenter = 256.0f;

// ----------------------------------------------------------------------------- rota

struct Route {
    std::vector<Vec3> points;
    float total = 0;
    void add(const Vec3& p) {
        if (!points.empty()) total += distXZ(points.back(), p);
        points.push_back(p);
    }
    // Posição a 'd' metros do início; vai e volta quando passa do fim.
    Vec3 at(float d) const {
        if (points.size() < 2 || total <= 0) return points.empty() ? Vec3() : points[0];
        float m = std::fmod(d, total * 2.0f);
        if (m > total) m = total * 2.0f - m;
        for (size_t i = 1; i < points.size(); ++i) {
            const float seg = distXZ(points[i - 1], points[i]);
            if (m <= seg || i + 1 == points.size()) {
                const float t = seg > 0 ? std::min(1.0f, m / seg) : 0.0f;
                return points[i - 1] + (points[i] - points[i - 1]) * t;
            }
            m -= seg;
        }
        return points.back();
    }
};

Route buildRoute(IslandGenerator& island) {
    Route r;
    r.add(island.start());
    for (int id : island.nests()) r.add(island.poiAtCell(id).center);
    if (island.portalId() >= 0) r.add(island.poiAtCell(island.portalId()).center);
    return r;
}

// ----------------------------------------------------------------------------- execução de uma política

struct PolicyResult {
    int frames = 0;
    double initMs = 0, totalMs = 0;
    long long gen = 0, mesh = 0, verts = 0;
    int starved = 0, pendMax = 0, over4 = 0, over8 = 0, over16 = 0;
    float strP50 = 0, strP95 = 0, strP99 = 0, strMax = 0;
    float genAvg = 0, genP95 = 0, genMax = 0, meshAvg = 0, meshP95 = 0, meshMax = 0;
    uint64_t hash = 0;
};

float avgOf(const std::vector<float>& v) {
    if (v.empty()) return 0;
    double s = 0;
    for (float x : v) s += x;
    return static_cast<float>(s / static_cast<double>(v.size()));
}

bool columnsAroundReady(const World& w, const Vec3& p) {
    for (int dz = -16; dz <= 16; dz += 16)
        for (int dx = -16; dx <= 16; dx += 16) {
            const int x = static_cast<int>(std::floor(p.x)) + dx, z = static_cast<int>(std::floor(p.z)) + dz;
            if (w.columnInBounds(x, z) && !w.columnReady(x, z)) return false;
        }
    return true;
}

PolicyResult runPolicy(const BenchConfig& cfg, int policy, int meters) {
    PolicyResult res;
    Theme theme = applySkyVariant(themeForStage(cfg.stage), cfg.seed, cfg.stage >= 3);
    IslandGenerator island(theme, cfg.stage, cfg.seed, true);
    World world;
    world.resetStreaming(kIslandSize, kIslandHeight, kIslandSize, &island);
    world.enableContentHash(true);
    StreamTrace trace;
    world.setTrace(&trace);

    const Route route = buildRoute(island);
    Vec3 pos = route.at(0);
    const double t0 = nowMs();
    for (int i = 0; i < 500; ++i)
        if (world.stream(pos.x, pos.z, cfg.radius, StreamBudget::bulk()).pending == 0) break;
    res.initMs = nowMs() - t0;
    trace.genMs.clear();
    trace.meshMs.clear();

    const float dt = 1.0f / 60.0f;
    const float step = cfg.speedMps * dt;
    const int frames = static_cast<int>(static_cast<float>(meters) / step);
    std::vector<float> str;
    str.reserve(static_cast<size_t>(frames) + 256);
    int pending = 0;  // replica o 'static int pending' do código antigo
    const double tRun = nowMs();
    for (int f = 0; f < frames + 200; ++f) {
        if (f < frames) pos = route.at(static_cast<float>(f) * step);
        StreamBudget b;
        if (policy == 0) {
            b = StreamBudget::legacy(pending > 24 ? 4 : 2, pending > 24 ? 6 : 3);
        } else {
            b.maxMs = std::max(1.0f, std::min(5.0f, 16.7f - cfg.assumedOtherMs - 3.0f));
        }
        const double s0 = nowMs();
        const StreamStats st = world.stream(pos.x, pos.z, cfg.radius, b);
        const float ms = static_cast<float>(nowMs() - s0);
        pending = st.pending;
        if (f < frames) {
            str.push_back(ms);
            if (ms > 4.0f) ++res.over4;
            if (ms > 8.0f) ++res.over8;
            if (ms > 16.0f) ++res.over16;
            res.pendMax = std::max(res.pendMax, st.pending);
            if (!columnsAroundReady(world, pos)) ++res.starved;
        }
        res.gen += st.generated;
        res.mesh += st.meshed;
    }
    res.totalMs = nowMs() - tRun;
    res.frames = frames;
    res.hash = world.residentHash(pos.x, pos.z, cfg.radius);
    for (const Chunk* c : world.chunks()) {
        const int dx = c->cx - (static_cast<int>(std::floor(pos.x)) >> 4), dz = c->cz - (static_cast<int>(std::floor(pos.z)) >> 4);
        if (c->meshed && dx * dx + dz * dz <= cfg.radius * cfg.radius) res.verts += static_cast<long long>(c->mesh.opaque.size() + c->mesh.cutout.size() + c->mesh.water.size());
    }
    if (!str.empty()) {
        std::vector<float> copy = str;
        res.strP50 = percentile(copy.data(), static_cast<int>(copy.size()), 0.50f);
        res.strP95 = percentile(copy.data(), static_cast<int>(copy.size()), 0.95f);
        res.strP99 = percentile(copy.data(), static_cast<int>(copy.size()), 0.99f);
        res.strMax = copy.back();
    }
    res.genAvg = avgOf(trace.genMs);
    res.meshAvg = avgOf(trace.meshMs);
    if (!trace.genMs.empty()) {
        res.genP95 = percentile(trace.genMs.data(), static_cast<int>(trace.genMs.size()), 0.95f);
        res.genMax = trace.genMs.back();
    }
    if (!trace.meshMs.empty()) {
        res.meshP95 = percentile(trace.meshMs.data(), static_cast<int>(trace.meshMs.size()), 0.95f);
        res.meshMax = trace.meshMs.back();
    }
    return res;
}

void logResult(const char* name, const PolicyResult& r) {
    ZZ_PERF_LOG(
        "bench result policy=%s frames=%d init.ms=%.0f gen=%lld mesh=%lld verts=%lld hash=0x%016llx starved=%d pend.max=%d "
        "str.p50=%.2f str.p95=%.2f str.p99=%.2f str.max=%.2f over4=%d over8=%d over16=%d gen.avg=%.2f gen.p95=%.2f gen.max=%.2f "
        "mesh.avg=%.2f mesh.p95=%.2f mesh.max=%.2f total.ms=%.0f",
        name, r.frames, r.initMs, r.gen, r.mesh, r.verts, static_cast<unsigned long long>(r.hash), r.starved, r.pendMax, r.strP50,
        r.strP95, r.strP99, r.strMax, r.over4, r.over8, r.over16, r.genAvg, r.genP95, r.genMax, r.meshAvg, r.meshP95, r.meshMax, r.totalMs);
}

// ----------------------------------------------------------------------------- autotestes

void testPercentile(TestLog& t) {
    float v[100];
    for (int i = 0; i < 100; ++i) v[i] = static_cast<float>(i + 1);
    float one[1] = {7.0f};
    t.report("ring_percentile", percentile(v, 100, 0.95f) == 95.0f && percentile(one, 1, 0.95f) == 7.0f && percentile(one, 0, 0.5f) == 0.0f);
}

void testProfiler(TestLog& t) {
    FrameProfiler p;
    for (int i = 0; i < 400; ++i) {
        const bool spike = i == 150 || i == 200 || i == 250 || i == 300;
        p.feedForTest(spike ? 80.0f : 16.7f, 8.0f);
    }
    p.forceSnapshot();
    const Snapshot& s = p.snapshot();
    t.report("profiler_window", s.frames == 300 && s.slow50 == 4 && s.intMax == 80.0f);
    const long long gapsBefore = p.gaps();
    p.feedForTest(900.0f, 8.0f);
    p.forceSnapshot();
    t.report("profiler_gap_ignored", p.gaps() == gapsBefore + 1 && p.snapshot().frames == 300);
}

void testBudgets(TestLog& t) {
    {  // legado: contagem
        FakeSource src;
        World w;
        w.resetStreaming(kFakeWorld, 64, kFakeWorld, &src);
        bool ok = true;
        for (int i = 0; i < 12; ++i) {
            const StreamStats st = w.stream(kCenter, kCenter, 6, StreamBudget::legacy(2, 3));
            ok = ok && st.generated <= 2 && st.meshed <= 3;
        }
        t.report("budget_legacy_counts", ok);
    }
    {  // piso: progride mesmo com orçamento zero
        FakeSource src;
        World w;
        w.resetStreaming(kFakeWorld, 64, kFakeWorld, &src);
        StreamBudget b;
        b.maxMs = 0;
        b.urgentRadius = -1;
        bool ok = true;
        int calls = 0;
        StreamStats st;
        for (; calls < 600; ++calls) {
            st = w.stream(kCenter, kCenter, 4, b);
            if (calls < 10) ok = ok && st.generated >= 1;
            if (st.pending == 0 && calls > 10) break;
        }
        t.report("budget_floor_progress", ok && st.pending == 0 && calls < 600);
    }
    {  // urgente ignora o tempo: 5x5 gerado na 1ª chamada, 3x3 malhado na 2ª
        FakeSource src;
        World w;
        w.resetStreaming(kFakeWorld, 64, kFakeWorld, &src);
        StreamBudget b;
        b.maxMs = 0;
        b.maxGenerate = 0;
        b.maxMesh = 0;
        b.minGenerate = b.minMesh = 0;
        b.urgentRadius = 1;
        w.stream(kCenter, kCenter, 6, b);
        bool gen = true, mesh = true;
        const int pc = static_cast<int>(kCenter) >> 4;
        for (int dz = -2; dz <= 2; ++dz)
            for (int dx = -2; dx <= 2; ++dx) {
                const Chunk* c = w.chunkAt(pc + dx, pc + dz);
                gen = gen && c && c->generated;
            }
        w.stream(kCenter, kCenter, 6, b);
        for (int dz = -1; dz <= 1; ++dz)
            for (int dx = -1; dx <= 1; ++dx) {
                const Chunk* c = w.chunkAt(pc + dx, pc + dz);
                mesh = mesh && c && c->meshed;
            }
        t.report("budget_urgent_ignores_time", gen && mesh);
    }
    {  // teto de segurança do trabalho urgente
        SlowSource src;
        World w;
        w.resetStreaming(kFakeWorld, 64, kFakeWorld, &src);
        StreamBudget b;
        b.maxMs = 0;
        b.minGenerate = b.minMesh = 0;
        b.urgentRadius = 1;
        b.urgentMaxMs = 6.0f;
        bool ok = true;
        int calls = 0;
        const int pc = static_cast<int>(kCenter) >> 4;
        for (; calls < 12; ++calls) {
            const StreamStats st = w.stream(kCenter, kCenter, 6, b);
            ok = ok && st.generated <= 5;
            bool done = true;
            for (int dz = -2; dz <= 2; ++dz)
                for (int dx = -2; dx <= 2; ++dx) {
                    const Chunk* c = w.chunkAt(pc + dx, pc + dz);
                    done = done && c && c->generated;
                }
            if (done) {
                ++calls;
                break;
            }
        }
        t.report("budget_urgent_cap", ok && calls <= 9);
    }
    {  // orçamento de tempo
        SlowSource src;
        World w;
        w.resetStreaming(kFakeWorld, 64, kFakeWorld, &src);
        StreamBudget b;
        b.maxMs = 3.0f;
        b.urgentRadius = -1;
        bool ok = true;
        for (int i = 0; i < 12; ++i) {
            const StreamStats st = w.stream(kCenter, kCenter, 6, b);
            ok = ok && st.generated <= 3;
        }
        t.report("budget_time_cap", ok);
    }
    {  // o resultado final não depende da política
        FakeSource a, b2;
        World wl, wb;
        wl.resetStreaming(kFakeWorld, 64, kFakeWorld, &a);
        wb.resetStreaming(kFakeWorld, 64, kFakeWorld, &b2);
        for (int i = 0; i < 1000; ++i)
            if (wl.stream(kCenter, kCenter, 5, StreamBudget::legacy(2, 3)).pending == 0) break;
        StreamBudget b;
        for (int i = 0; i < 1000; ++i)
            if (wb.stream(kCenter, kCenter, 5, b).pending == 0) break;
        const uint64_t h1 = wl.residentHash(kCenter, kCenter, 5), h2 = wb.residentHash(kCenter, kCenter, 5);
        t.report("budget_completes_equal_legacy", h1 == h2 && h1 != 0);
    }
}

void testIsland(TestLog& t, const BenchConfig& cfg) {
    BenchConfig c = cfg;
    c.radius = 4;
    c.meters = 300;
    const PolicyResult a = runPolicy(c, 0, c.meters);
    const PolicyResult b = runPolicy(c, 1, c.meters);
    const PolicyResult a2 = runPolicy(c, 0, c.meters);
    t.report("no_starvation_route", b.starved == 0);
    t.report("hash_deterministic", a.hash == a2.hash && a.hash == b.hash && a.hash != 0);
}

}  // namespace

int runBench(const BenchConfig& cfg) {
    ZZ_PERF_LOG("bench begin seed=%u stage=%d radius=%d meters=%d speed=%.1f", cfg.seed, cfg.stage, cfg.radius, cfg.meters, cfg.speedMps);
    TestLog tests;
    if (cfg.selfTest) {
        testPercentile(tests);
        testProfiler(tests);
        testBudgets(tests);
        testIsland(tests, cfg);
    }
    int rc = tests.failed ? 1 : 0;
    PolicyResult legacy, budget;
    bool haveLegacy = false, haveBudget = false;
    if (cfg.policy == 0 || cfg.policy == 2) {
        legacy = runPolicy(cfg, 0, cfg.meters);
        haveLegacy = true;
        logResult("legacy", legacy);
    }
    if (cfg.policy == 1 || cfg.policy == 2) {
        budget = runPolicy(cfg, 1, cfg.meters);
        haveBudget = true;
        logResult("budget", budget);
        if (budget.starved != 0) rc = 1;
    }
    if (haveLegacy && haveBudget) {
        const bool eq = legacy.hash == budget.hash;
        ZZ_PERF_LOG("bench compare hashEqual=%d str.p99.delta=%.1f over8.delta=%d", eq ? 1 : 0, budget.strP99 - legacy.strP99, budget.over8 - legacy.over8);
        if (!eq) rc = 1;
    }
    ZZ_PERF_LOG("bench done rc=%d failed=%d", rc, tests.failed);
    return rc;
}

}  // namespace zz::perf
