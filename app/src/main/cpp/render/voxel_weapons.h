// Armas e objetos de mão em blocos (estilo voxel), montados em código: caixas, perfis extrudados e
// texturas pixeladas de madeira/metal geradas aqui mesmo (sem imagens externas). A AK segue o modelo
// do visualizador "AK-47 Voxel" do usuário; escopeta, granadas e faca usam as mesmas paletas.
#pragma once

#include "../core/math.h"

namespace zz {

struct CharacterAsset;

enum class VoxelItem { Rifle, Shotgun, Grenade, Smoke, Molotov, Knife, Pistol, Smg, Sniper, Count };

// Monta malha e textura do item no espaço da mão (x = direita, y = cima, z = cano para a frente).
void buildVoxelItem(VoxelItem item, CharacterAsset& out);

// Ponta do cano no espaço da mão (clarão e saída dos projéteis).
Vec3 voxelMuzzle(VoxelItem item);

// Mira dianteira no espaço da mão (brilho da munição incendiária).
Vec3 voxelFrontSight(VoxelItem item);

// Pontos onde a arma é segura com as duas mãos (espaço da mão): empunhadura, guarda-mão e soleira da coronha.
Vec3 voxelGrip(VoxelItem item);
Vec3 voxelForegrip(VoxelItem item);
Vec3 voxelButt(VoxelItem item);

}  // namespace zz
