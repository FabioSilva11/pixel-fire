// Personagem voxel detalhado do jogador: esqueleto humano com os nomes do Mixamo, mãos com cinco
// dedos e texturas pixeladas iguais às do protótipo tools/personagem-voxel (mesmo ruído e paletas).
// O esqueleto repousa com os braços para baixo, como os clipes procedurais antigos esperam; as animações
// do Mixamo chegam já convertidas para esse repouso (tools/anim-converter).
#pragma once

#include <cstdint>
#include <vector>

#include "../core/platform.h"
#include "character.h"

namespace zz {

struct Look;

// Ossos que os clipes procedurais movem (as sete partes antigas, mais joelhos, cotovelos e pés).
struct HumanoidRig {
    int root = -1, torso = -1, head = -1, armR = -1, armL = -1, legR = -1, legL = -1;
    int foreR = -1, foreL = -1, kneeR = -1, kneeL = -1, footR = -1, footL = -1;
};

// Monta nós, malhas (uma por osso) e a textura do personagem.
void buildVoxelHumanoid(const Look& look, uint32_t seed, std::vector<ModelNode>& nodes, std::vector<ModelMesh>& meshes,
                        Image& image, HumanoidRig& rig);

// Clipes do herói iguais às poses da página de teste: parado com a arma pronta (pernas afastadas, joelhos
// dobrados), andar, correr e agachar de verdade (quadril baixa, joelhos dobram). Substituem os de mesmo nome.
void addHeroClips(const std::vector<ModelNode>& nodes, std::vector<AnimClip>& clips);

// Acrescenta os clipes convertidos do Mixamo (assets/anims/*.zza), substituindo os de mesmo nome.
void addMixamoClips(const std::vector<ModelNode>& nodes, std::vector<AnimClip>& clips);

// Nomes antigos das partes (arm-right, torso, ...) apontando para os ossos novos.
void addHumanoidAliases(CharacterModel& model);

}  // namespace zz
