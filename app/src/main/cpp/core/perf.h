// Diagnóstico de desempenho sempre ligado: janela deslizante de tempos de quadro, tempo por seção,
// contadores do mundo/renderer e log periódico (tag ZZPerf). Independente do motor: compila em teste de host
// (defina ZZ_PERF_HOST para trocar o log do Android por stderr).
#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

#if defined(__ANDROID__) && !defined(ZZ_PERF_HOST)
#include <android/log.h>
#include <android/trace.h>
#define ZZ_PERF_LOG(...) __android_log_print(ANDROID_LOG_INFO, "ZZPerf", __VA_ARGS__)
#define ZZ_TRACE_BEGIN(name) ATrace_beginSection(name)
#define ZZ_TRACE_END() ATrace_endSection()
#else
#define ZZ_PERF_LOG(...) (std::fprintf(stderr, __VA_ARGS__), std::fputc('\n', stderr))
#define ZZ_TRACE_BEGIN(name) ((void)0)
#define ZZ_TRACE_END() ((void)0)
#endif

#define ZZ_CAT2(a, b) a##b
#define ZZ_CAT(a, b) ZZ_CAT2(a, b)
// Seção visível no Perfetto/systrace; custo desprezível com o trace desligado.
#define ZZ_TRACE_SCOPE(name) ::zz::perf::TraceScope ZZ_CAT(zzTrace_, __LINE__)(name)

namespace zz::perf {

inline double nowMs() {
    using clock = std::chrono::steady_clock;  // CLOCK_MONOTONIC no Android, o mesmo de System.nanoTime()
    return std::chrono::duration<double, std::milli>(clock::now().time_since_epoch()).count();
}

struct TraceScope {
    explicit TraceScope(const char* name) { ZZ_TRACE_BEGIN(name); }
    ~TraceScope() { ZZ_TRACE_END(); }
};

// Seções medidas (tempo INCLUSIVO em ms). Update é derivado: trabalho - (Scene + Render + Hud),
// portanto inclui Stream, Nav e Minimap. Upload está contido em Render.
enum class Sec : int { Update, Stream, Nav, Minimap, Scene, Render, Upload, Hud, Count };
enum class Cnt : int { Generated, Meshed, Unloaded, Uploads, UploadBytes, ChunkDraws, CharDraws, BoxDraws, BillboardDraws, Culled, Count };
constexpr int kSecCount = static_cast<int>(Sec::Count);
constexpr int kCntCount = static_cast<int>(Cnt::Count);
inline const char* secKey(Sec s) {
    static const char* k[kSecCount] = {"upd", "str", "nav", "mini", "scene", "rend", "upl", "hud"};
    return k[static_cast<int>(s)];
}

// p em [0,1]; ordena 'v' no lugar.
inline float percentile(float* v, int n, float p) {
    if (n <= 0) return 0.0f;
    std::sort(v, v + n);
    int k = static_cast<int>(p * static_cast<float>(n) + 0.999f) - 1;
    return v[std::max(0, std::min(n - 1, k))];
}

struct Snapshot {
    int frames = 0;                      // amostras na janela
    float windowSec = 0;
    float fps = 0;                       // frames / duração da janela
    float intAvg = 0, intP50 = 0, intP95 = 0, intMax = 0;   // intervalo entre quadros (ms)
    float workAvg = 0, workP95 = 0, workMax = 0;            // CPU dentro de Game::frame (ms)
    float sec[kSecCount] = {};           // média por quadro (ms)
    float secMax[kSecCount] = {};
    int slow33 = 0, slow50 = 0;          // quadros com intervalo > 33,3 / > 50 ms
};

class FrameProfiler {
public:
    static constexpr int kRing = 300;    // ~5 s a 60 fps
    static constexpr float kHitchIntervalMs = 50.0f, kHitchWorkMs = 25.0f;
    static constexpr double kLogEveryMs = 5000.0, kSnapEveryMs = 1000.0;
    static constexpr float kGapMs = 500.0f;

    void begin(double nowSec);           // início de Game::frame; nowSec = relógio do Kotlin
    void end();                          // fim (via FrameGuard, cobre retornos antecipados)

    void add(Sec s, double ms) { sec_[static_cast<int>(s)] += static_cast<float>(ms); }
    void count(Cnt c, long long n = 1) { cnt_[static_cast<int>(c)] += n; }
    // Valores instantâneos para o log/HUD (chame antes de end()).
    void gauge(int pendingGen, int pendingUrgent, int pendingUploads, int chunks, float gpuMB, int flags);
    void setContext(int mode, int quality) { mode_ = mode; quality_ = quality; }
    void skipFrames(int n) { skip_ = n; }        // descarta quadros de carga/transição

    // Execução reproduzível: acumula TODOS os quadros até endRun() e loga p50/p95/p99/máx globais.
    void startRun(const char* label);
    void endRun(const char* extra = "");     // 'extra' = campos adicionais "chave=valor" anexados à linha

    const Snapshot& snapshot() const { return snap_; }
    float otherMs() const { return otherEma_; }          // EMA de (trabalho - streaming), para adaptar o orçamento
    float genPerSecond() const { return genPs_; }
    float uploadsPerSecond() const { return upPs_; }
    bool sustainedSlow() const { return slowSeconds_ >= 10; }   // p95 > 25 ms por >= 10 snapshots seguidos

    // Alimenta um quadro sintético (autotestes): interval/work em ms.
    void feedForTest(float intervalMs, float workMs);
    long long gaps() const { return gaps_; }
    void forceSnapshot() { summarize(snap_); }

private:
    void pushFrame(float interval, float work);
    void summarize(Snapshot& out) const;
    void logWindow(double nowMs);
    void logHitch(float interval, float work);

    double t0_ = 0, lastNow_ = -1, lastLogMs_ = 0, lastSnapMs_ = 0, lastHitchMs_ = -1e9;
    float interval_ = 0;
    float sec_[kSecCount] = {};
    long long cnt_[kCntCount] = {};
    // Anéis (cronológicos): intervalo, trabalho, seções.
    float ringInt_[kRing] = {}, ringWork_[kRing] = {}, ringSec_[kRing][kSecCount] = {};
    int head_ = 0, n_ = 0, skip_ = 0;
    long long gaps_ = 0;
    // Totais da janela de log (zeram a cada logWindow).
    long long tot_[kCntCount] = {}, max_[kCntCount] = {};
    int pendingMax_ = 0;
    // Valores instantâneos.
    int pendGen_ = 0, pendUrgent_ = 0, pendUp_ = 0, chunks_ = 0, flags_ = 0, mode_ = 0, quality_ = 0;
    float gpuMB_ = 0, otherEma_ = 8.0f, genPs_ = 0, upPs_ = 0;
    int slowSeconds_ = 0;
    Snapshot snap_;
    // Execução reproduzível.
    bool running_ = false;
    char runLabel_[48] = {};
    std::vector<float> runInt_, runWork_, runStream_;
    long long runTot_[kCntCount] = {};
    double runStartMs_ = 0;
};

class FrameGuard {
public:
    FrameGuard(FrameProfiler& p, double nowSec) : p_(p) { p_.begin(nowSec); }
    ~FrameGuard() { p_.end(); }
    FrameGuard(const FrameGuard&) = delete;
    FrameGuard& operator=(const FrameGuard&) = delete;
private:
    FrameProfiler& p_;
};

class Scoped {
public:
    Scoped(FrameProfiler& p, Sec s) : p_(p), s_(s), t0_(nowMs()) {}
    ~Scoped() { p_.add(s_, nowMs() - t0_); }
    Scoped(const Scoped&) = delete;
    Scoped& operator=(const Scoped&) = delete;
private:
    FrameProfiler& p_;
    Sec s_;
    double t0_;
};

}  // namespace zz::perf
