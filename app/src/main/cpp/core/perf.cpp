#include "perf.h"

#include <cstring>

namespace zz::perf {

void FrameProfiler::begin(double nowSec) {
    t0_ = nowMs();
    interval_ = lastNow_ < 0 ? 0.0f : static_cast<float>((nowSec - lastNow_) * 1000.0);
    lastNow_ = nowSec;
    std::memset(sec_, 0, sizeof(sec_));
    std::memset(cnt_, 0, sizeof(cnt_));
}

void FrameProfiler::gauge(int pendingGen, int pendingUrgent, int pendingUploads, int chunks, float gpuMB, int flags) {
    pendGen_ = pendingGen;
    pendUrgent_ = pendingUrgent;
    pendUp_ = pendingUploads;
    chunks_ = chunks;
    gpuMB_ = gpuMB;
    flags_ = flags;
}

void FrameProfiler::end() {
    const float work = static_cast<float>(nowMs() - t0_);
    const int iUpd = static_cast<int>(Sec::Update), iScene = static_cast<int>(Sec::Scene);
    const int iRend = static_cast<int>(Sec::Render), iHud = static_cast<int>(Sec::Hud);
    const int iStr = static_cast<int>(Sec::Stream);
    sec_[iUpd] = std::max(0.0f, work - sec_[iScene] - sec_[iRend] - sec_[iHud]);
    otherEma_ += ((work - sec_[iStr]) - otherEma_) * 0.1f;
    if (skip_ > 0) {
        --skip_;
        return;
    }
    if (interval_ <= 0 || interval_ > kGapMs) {
        ++gaps_;
        return;
    }
    pushFrame(interval_, work);
}

void FrameProfiler::feedForTest(float intervalMs, float workMs) {
    if (intervalMs <= 0 || intervalMs > kGapMs) {
        ++gaps_;
        return;
    }
    pushFrame(intervalMs, workMs);
}

void FrameProfiler::pushFrame(float interval, float work) {
    ringInt_[head_] = interval;
    ringWork_[head_] = work;
    std::memcpy(ringSec_[head_], sec_, sizeof(sec_));
    head_ = (head_ + 1) % kRing;
    if (n_ < kRing) ++n_;
    for (int i = 0; i < kCntCount; ++i) {
        tot_[i] += cnt_[i];
        max_[i] = std::max(max_[i], cnt_[i]);
    }
    pendingMax_ = std::max(pendingMax_, pendGen_);
    if (running_) {
        runInt_.push_back(interval);
        runWork_.push_back(work);
        runStream_.push_back(sec_[static_cast<int>(Sec::Stream)]);
        for (int i = 0; i < kCntCount; ++i) runTot_[i] += cnt_[i];
    }
    const double now = nowMs();
    if ((interval > kHitchIntervalMs || work > kHitchWorkMs) && now - lastHitchMs_ >= 250.0) {
        lastHitchMs_ = now;
        logHitch(interval, work);
    }
    if (now - lastSnapMs_ >= kSnapEveryMs) {
        lastSnapMs_ = now;
        summarize(snap_);
        const double elapsed = std::max(1.0, now - lastLogMs_) / 1000.0;
        genPs_ = static_cast<float>(tot_[static_cast<int>(Cnt::Generated)] / elapsed);
        upPs_ = static_cast<float>(tot_[static_cast<int>(Cnt::Uploads)] / elapsed);
        if (snap_.intP95 > 25.0f) ++slowSeconds_;
        else slowSeconds_ = 0;
    }
    if (now - lastLogMs_ >= kLogEveryMs) {
        logWindow(now);
        lastLogMs_ = now;
        std::memset(tot_, 0, sizeof(tot_));
        std::memset(max_, 0, sizeof(max_));
        pendingMax_ = 0;
        gaps_ = 0;
    }
}

void FrameProfiler::summarize(Snapshot& out) const {
    out = Snapshot();
    const int n = n_;
    if (n <= 0) return;
    float a[kRing], b[kRing];
    double sumInt = 0, sumWork = 0, sumSec[kSecCount] = {};
    for (int i = 0; i < n; ++i) {
        const int idx = (head_ - n + i + kRing * 2) % kRing;
        const float iv = ringInt_[idx], wk = ringWork_[idx];
        a[i] = iv;
        b[i] = wk;
        sumInt += iv;
        sumWork += wk;
        out.intMax = std::max(out.intMax, iv);
        out.workMax = std::max(out.workMax, wk);
        if (iv > 33.3f) ++out.slow33;
        if (iv > 50.0f) ++out.slow50;
        for (int s = 0; s < kSecCount; ++s) {
            sumSec[s] += ringSec_[idx][s];
            out.secMax[s] = std::max(out.secMax[s], ringSec_[idx][s]);
        }
    }
    out.frames = n;
    out.windowSec = static_cast<float>(sumInt / 1000.0);
    out.fps = sumInt > 0 ? static_cast<float>(n / (sumInt / 1000.0)) : 0.0f;
    out.intAvg = static_cast<float>(sumInt / n);
    out.workAvg = static_cast<float>(sumWork / n);
    for (int s = 0; s < kSecCount; ++s) out.sec[s] = static_cast<float>(sumSec[s] / n);
    out.intP50 = percentile(a, n, 0.5f);
    out.intP95 = percentile(a, n, 0.95f);
    out.workP95 = percentile(b, n, 0.95f);
}

void FrameProfiler::logWindow(double now) {
    Snapshot s;
    summarize(s);
    if (s.frames <= 0) return;
    const double secs = std::max(1.0, now - lastLogMs_) / 1000.0;
    auto ps = [&](Cnt c) { return static_cast<float>(tot_[static_cast<int>(c)] / secs); };
    auto avgPerFrame = [&](Cnt c) { return static_cast<float>(tot_[static_cast<int>(c)]) / static_cast<float>(s.frames); };
    auto mx = [&](Cnt c) { return static_cast<long long>(max_[static_cast<int>(c)]); };
    ZZ_PERF_LOG(
        "win n=%d fps=%.1f int.avg=%.1f int.p50=%.1f int.p95=%.1f int.max=%.1f work.avg=%.1f work.p95=%.1f work.max=%.1f "
        "s.upd=%.1f s.str=%.1f s.nav=%.1f s.mini=%.1f s.scene=%.1f s.rend=%.1f s.upl=%.1f s.hud=%.1f "
        "sm.str=%.1f sm.nav=%.1f sm.mini=%.1f sm.upl=%.1f slow33=%d slow50=%d gaps=%lld "
        "gen.ps=%.1f gen.max=%lld mesh.ps=%.1f mesh.max=%lld unl.ps=%.1f up.ps=%.1f up.max=%lld upKB.ps=%.0f "
        "pend=%d pend.max=%d pendUrg=%d chunks=%d gpuMB=%.1f dc.chunks=%.0f dc.chars=%.0f dc.boxes=%.0f dc.bb=%.0f cull=%.0f "
        "mode=%d q=%d flags=%d",
        s.frames, s.fps, s.intAvg, s.intP50, s.intP95, s.intMax, s.workAvg, s.workP95, s.workMax,
        s.sec[0], s.sec[1], s.sec[2], s.sec[3], s.sec[4], s.sec[5], s.sec[6], s.sec[7],
        s.secMax[1], s.secMax[2], s.secMax[3], s.secMax[6], s.slow33, s.slow50, gaps_,
        ps(Cnt::Generated), mx(Cnt::Generated), ps(Cnt::Meshed), mx(Cnt::Meshed), ps(Cnt::Unloaded), ps(Cnt::Uploads),
        mx(Cnt::Uploads), ps(Cnt::UploadBytes) / 1024.0f,
        pendGen_, pendingMax_, pendUrgent_, chunks_, gpuMB_, avgPerFrame(Cnt::ChunkDraws), avgPerFrame(Cnt::CharDraws),
        avgPerFrame(Cnt::BoxDraws), avgPerFrame(Cnt::BillboardDraws), avgPerFrame(Cnt::Culled),
        mode_, quality_, flags_);
}

void FrameProfiler::logHitch(float interval, float work) {
    // Seção culpada: maior entre str, nav, mini, scene, rend, upl, hud.
    const Sec cands[] = {Sec::Stream, Sec::Nav, Sec::Minimap, Sec::Scene, Sec::Render, Sec::Upload, Sec::Hud};
    Sec top = Sec::Stream;
    float best = -1;
    for (Sec c : cands) {
        const float v = sec_[static_cast<int>(c)];
        if (v > best) {
            best = v;
            top = c;
        }
    }
    auto sv = [&](Sec s) { return sec_[static_cast<int>(s)]; };
    ZZ_PERF_LOG("hitch int=%.1f work=%.1f top=%s:%.1f str=%.1f upd=%.1f nav=%.1f mini=%.1f scene=%.1f rend=%.1f upl=%.1f gen=%lld mesh=%lld up=%lld pend=%d chunks=%d",
                interval, work, secKey(top), best, sv(Sec::Stream), sv(Sec::Update), sv(Sec::Nav), sv(Sec::Minimap),
                sv(Sec::Scene), sv(Sec::Render), sv(Sec::Upload), cnt_[static_cast<int>(Cnt::Generated)],
                cnt_[static_cast<int>(Cnt::Meshed)], cnt_[static_cast<int>(Cnt::Uploads)], pendGen_, chunks_);
}

void FrameProfiler::startRun(const char* label) {
    runInt_.clear();
    runWork_.clear();
    runStream_.clear();
    runInt_.reserve(8192);
    runWork_.reserve(8192);
    runStream_.reserve(8192);
    std::memset(runTot_, 0, sizeof(runTot_));
    std::snprintf(runLabel_, sizeof(runLabel_), "%s", label ? label : "run");
    running_ = true;
    runStartMs_ = nowMs();
}

void FrameProfiler::endRun(const char* extra) {
    if (!running_) return;
    running_ = false;
    const int n = static_cast<int>(runInt_.size());
    if (n <= 0) return;
    int slow33 = 0, slow50 = 0;
    float intMax = 0, workMax = 0, strMax = 0;
    for (int i = 0; i < n; ++i) {
        if (runInt_[i] > 33.3f) ++slow33;
        if (runInt_[i] > 50.0f) ++slow50;
        intMax = std::max(intMax, runInt_[i]);
        workMax = std::max(workMax, runWork_[i]);
        strMax = std::max(strMax, runStream_[i]);
    }
    const float i50 = percentile(runInt_.data(), n, 0.50f), i95 = percentile(runInt_.data(), n, 0.95f),
                i99 = percentile(runInt_.data(), n, 0.99f);
    const float w50 = percentile(runWork_.data(), n, 0.50f), w95 = percentile(runWork_.data(), n, 0.95f),
                w99 = percentile(runWork_.data(), n, 0.99f);
    const float s95 = percentile(runStream_.data(), n, 0.95f), s99 = percentile(runStream_.data(), n, 0.99f);
    ZZ_PERF_LOG(
        "run done label=%s frames=%d sec=%.1f int.p50=%.1f int.p95=%.1f int.p99=%.1f int.max=%.1f work.p50=%.1f work.p95=%.1f "
        "work.p99=%.1f work.max=%.1f str.p95=%.1f str.p99=%.1f str.max=%.1f slow33=%d slow50=%d gen=%lld mesh=%lld up=%lld upMB=%.1f pend.max=%d %s",
        runLabel_, n, (nowMs() - runStartMs_) / 1000.0, i50, i95, i99, intMax, w50, w95, w99, workMax, s95, s99, strMax,
        slow33, slow50, runTot_[static_cast<int>(Cnt::Generated)], runTot_[static_cast<int>(Cnt::Meshed)],
        runTot_[static_cast<int>(Cnt::Uploads)], runTot_[static_cast<int>(Cnt::UploadBytes)] / (1024.0 * 1024.0), pendingMax_,
        extra ? extra : "");
}

}  // namespace zz::perf
