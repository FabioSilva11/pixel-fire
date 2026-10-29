// Campo de fluxo 2.5D (BFS a partir do jogador) para os zumbis contornarem obstáculos e relevo.
// Cobre uma janela móvel ao redor do jogador (o mundo das fases é grande demais para uma grade inteira).
#pragma once

#include <cstdint>
#include <vector>

#include "../core/math.h"
#include "../world/world.h"

namespace zz {

class NavField {
public:
    static constexpr uint16_t kUnreached = 0xFFFF;
    static constexpr int kWindow = 128;

    // Recalcula alturas e distâncias numa janela centrada no alvo.
    void compute(const World& world, int targetX, int targetZ, int maxDistance);
    uint16_t distanceAt(int x, int z) const {
        int lx = x - ox_, lz = z - oz_;
        if (lx < 0 || lz < 0 || lx >= w_ || lz >= d_) return kUnreached;
        return dist_[static_cast<size_t>(lz) * w_ + lx];
    }
    // Próximo ponto (centro de coluna vizinha) rumo ao alvo; false se não houver caminho.
    bool nextStep(const Vec3& from, Vec3& out) const;
    bool walkable(int x, int z) const {
        int lx = x - ox_, lz = z - oz_;
        return lx >= 0 && lz >= 0 && lx < w_ && lz < d_ && !blocked_[static_cast<size_t>(lz) * w_ + lx];
    }
    int heightAt(int x, int z) const {
        int lx = x - ox_, lz = z - oz_;
        return (lx >= 0 && lz >= 0 && lx < w_ && lz < d_) ? height_[static_cast<size_t>(lz) * w_ + lx] : 0;
    }

private:
    bool canMove(int ax, int az, int bx, int bz) const;

    int ox_ = 0, oz_ = 0, w_ = 0, d_ = 0;
    std::vector<int16_t> height_;
    std::vector<uint8_t> blocked_;
    std::vector<uint16_t> dist_;
    std::vector<int> queue_;
};

}  // namespace zz
