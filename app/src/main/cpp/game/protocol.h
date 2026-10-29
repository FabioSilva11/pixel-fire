// Protocolo compartilhado com o Kotlin (NativeProtocol.kt deve espelhar estes índices).
#pragma once

namespace zz::proto {

// Configuração da missão (FloatArray vindo do Kotlin).
enum ConfigIndex {
    C_STAGE = 0, C_SKIN, C_ZOMBIES, C_ZOMBIE_HP, C_ZOMBIE_DMG, C_ZOMBIE_SPEED, C_PLAYER_HP, C_MAGAZINE,
    C_POWER, C_CRYSTAL_MASK, C_CRYSTAL_TOTAL, C_SENSITIVITY, C_AIM_ASSIST, C_INVERT_Y, C_SEED,
    C_RELOAD_SPEED, C_BOOST_HEALTH, C_BOOST_DAMAGE, C_QUALITY,
    // Modo e melhorias
    C_MODE,          // 0 campanha, 1 sobrevivência por ondas
    C_ARMOR,         // redução de dano 0..0.6
    C_REGEN,         // fração da vida máxima regenerada por segundo
    C_SPEED,         // multiplicador de velocidade
    C_HEADSHOT,      // multiplicador extra de tiro na cabeça
    C_LUCK,          // multiplicador da chance de itens
    C_EXPLOSIVES = 27,  // nível de explosivos (dano e pilha de granadas)
    // Diagnóstico de desempenho (somente builds de depuração enviam valores diferentes de zero)
    C_PERF = 28,        // 0 desligado, 1 autopiloto sem inimigos, 2 autopiloto com inimigos
    C_PERF_FRAMES = 29, // duração em quadros simulados (0 = 7200)
    C_COUNT = 56,
    // Contrato de expedição (etapa 4): bloco reservado 40..55. 0 = neutro/ilimitado; o JNI traduz.
    C_CONTRACT_ID = 40,   // 0 nenhum, 1 silenciosa, 2 ninho instável, 3 noite vermelha, 4 caixa perdida, 5 resistência
    C_AMMO_CLIPS,         // 41 reserva de munição em pentes (0 = ilimitada)
    C_CHEST_AMMO,         // 42 reserva por baú, em centésimos de pente
    C_VISION_MUL,         // 43 x alcance da névoa (0 = 1)
    C_AGGRESSION,         // 44 x percepção e persistência dos zumbis (0 = 1)
    C_RED_SKY,            // 45 1 = céu da noite vermelha
    C_GUARD_MUL,          // 46 x guardas dos pontos de interesse (0 = 1)
    C_NEST_BOMB,          // 47 (4C) chance de o ninho gerar Inchado
    C_CARGO               // 48 (4D) 1 = existe carga opcional
};

// Inventário (IntArray vindo do Kotlin): (id, quantidade) x 24 + espaço da arma + espaço do arremessável.
constexpr int kInvIntCount = 24 * 2 + 2;

// Estado do HUD (FloatArray lido pelo Kotlin).
enum HudIndex {
    H_HEALTH = 0, H_MAX_HEALTH, H_AMMO, H_MAG, H_RELOAD, H_ALIVE, H_TOTAL, H_PORTAL_OPEN, H_CRYSTALS,
    H_ELAPSED, H_TARGET_LOCKED, H_TARGET_HP, H_AIM, H_SPREAD, H_HITMARK, H_HEADSHOT_MARK, H_DAMAGE_FLASH,
    H_PLAYER_X, H_PLAYER_Z, H_CAM_YAW, H_PORTAL_X, H_PORTAL_Z, H_SPRINTING, H_JUMP_READY, H_KILLS,
    H_HEADSHOTS, H_DAMAGE_DIR, H_DAMAGE_DIR_T, H_PORTAL_SX, H_PORTAL_SY, H_PORTAL_ONSCREEN, H_FIRING,
    H_WORLD_SIZE, H_STATE, H_IN_WATER, H_FOV, H_FPS, H_CRYSTAL_TOTAL, H_COMBO,
    H_WEAPON_READY = 39,  // 1 = mira branca (pode atirar), 0 = cinza (recarregando ou entre disparos)
    H_ENEMY_COUNT = 40, H_ENEMIES = 41,           // (x, z, alertado) x 40
    H_LABEL_COUNT = 161, H_LABELS = 162,          // (sx, sy, tipo, valor, alfa) x 24
    H_BAR_COUNT = 282, H_BARS = 283,              // (sx, sy, fração, alfa, elite) x 16
    // Bloco da ilha (missões)
    H_OBJ_COUNT = 500, H_OBJ = 501,               // (x, z, estado, sx, sy, na tela) x 4 — estado: 1 ninho, 2 destruído, 3 portal fechado, 4 portal aberto
    H_NESTS_DONE = 525, H_NEST_TOTAL, H_NEST_HP, H_BURNING, H_BLEEDING, H_POISON, H_SLOWED, H_BOOST,
    H_SUPPLY, H_DISTANCE, H_CHESTS, H_CHEST_DIAMONDS, H_ISLAND, H_SHAKE, H_NEST_NEAR, H_SUPPLY_X, H_SUPPLY_Z,
    // Controles completos
    H_CROUCH = 542, H_GRENADES, H_MELEE, H_SHOULDER, H_SPRINT_LOCK,
    // Inventário: (id, quantidade) x 24
    H_INV = 560,
    H_INV_ENABLED = 608, H_WEAPON_SLOT, H_THROW_SLOT, H_WEAPON, H_ARMOR, H_ARMOR_MAX, H_ADRENALINE,
    H_THROW_AIM, H_THROW_DIST, H_SWAP, H_THROW_KIND, H_THROW_SX, H_THROW_SY, H_THROW_ONSCREEN,
    H_CLIPS = 622,  // 622..629 munição no pente de cada arma (id 1..8); -1 = pente cheio (nunca equipada)
    // Modo sobrevivência
    H_SURV = 630, H_WAVE, H_WAVE_STATE, H_WAVE_TIMER, H_WAVE_PROGRESS, H_WAVE_LEFT, H_WAVE_BIG, H_SURV_TIME,
    H_SCORE, H_FRONT_COUNT, H_FRONTS = 640,       // ângulo de cada frente de ataque x 3
    H_WAVE_FLAGS = 643,                            // posição (0..1) das grandes hordas no progresso desta onda
    H_MAG_MUL = 644,   // multiplicador do pente (melhoria/fase): pente = lround(base * H_MAG_MUL), como magazineFor()
    // Desempenho (somente leitura pelo HUD; 645-649 livres)
    H_PERF_FPS = 650,        // quadros/s na janela deslizante (~5 s)
    H_PERF_AVG_MS = 651,     // intervalo médio entre quadros
    H_PERF_P95_MS = 652,     // p95 do intervalo
    H_PERF_MAX_MS = 653,     // pior intervalo da janela
    H_PERF_WORK_MS = 654,    // CPU média dentro de Game::frame
    H_PERF_STREAM_MS = 655,  // média de tempo de streaming de mundo por quadro
    H_PERF_PENDING = 656,    // chunks pendentes de geração/malha
    H_PERF_GEN_PS = 657,     // chunks gerados por segundo
    H_PERF_UP_PS = 658,      // uploads de malha por segundo
    H_PERF_FLAGS = 659,      // bit0 streaming ativo, bit1 orçamento esgotado, bit2 degradação sustentada (p95>25 ms por 10 s), bit3 autopiloto
    // Cristais caídos e ainda não recolhidos (objetivo/tutorial): contagem + (x, z, sx, sy, naTela) x 3
    H_CRYSTAL_COUNT = 660, H_CRYSTAL_ITEMS = 661,   // 661..675
    // Contrato de expedição (etapa 4): bloco reservado 700..719. Só vale quando H_CT_ID != 0.
    H_CT_ID = 700,        // ContractId da missão (0 = sem contrato)
    H_CT_RESERVE_ROUNDS,  // 701 balas de reserva para a arma em mãos (-1 = ilimitada)
    H_CT_RESERVE_CLIPS,   // 702 pentes de reserva (-1 = ilimitada)
    H_CT_ALERTS,          // 703 zumbis alertados até agora
    H_CT_CARGO_STATE,     // 704 (4D) 0 sem carga, 1 oculta, 2 revelada, 3 coletada
    H_CT_CARGO_X, H_CT_CARGO_Z,                         // 705, 706 (só com a carga revelada)
    H_CT_CARGO_SX, H_CT_CARGO_SY, H_CT_CARGO_ON,        // 707..709 projeção na tela (0..1) e visibilidade; com a carga OCULTA, SX = dica de distância em metros (múltiplo de 100)
    H_CT_BOMBS,           // 710 (4C) Inchados gerados por ninhos até agora
    H_SIZE = 720
};
constexpr int kHudCrystalStride = 5;
constexpr int kMaxHudCrystals = 3;   // um cristal por ninho (kNestCount); aumentar junto com kMaxHudObjectives
static_assert(H_CRYSTAL_ITEMS + kMaxHudCrystals * kHudCrystalStride <= H_SIZE, "bloco de cristais cabe no HUD");
constexpr int kHudBarStride = 5;
constexpr int kHudObjStride = 6;
constexpr int kMaxHudObjectives = 4;

constexpr int kMaxHudEnemies = 40;
constexpr int kMaxHudLabels = 24;
constexpr int kMaxHudBars = 16;

enum EventType {
    EV_SFX = 1,          // a = som, b = volume 0..100, c = pan -100..100
    EV_KILL = 2,         // a = tiro na cabeça, b = tipo do zumbi, c = combo
    EV_CRYSTAL = 3,      // a = índice do cristal
    EV_PORTAL_OPEN = 4,
    EV_VICTORY = 5,
    EV_DEFEAT = 6,
    EV_PLAYER_HURT = 7,  // a = dano
    EV_MISSION_READY = 8, // a = estilo do mapa (-1 = ilha)
    EV_HEAL = 9,
    EV_NEST_DESTROYED = 14, // a = índice do ninho
    EV_CHEST = 15,        // a = diamantes, b = 1 se veio granada
    EV_POI = 16,          // a = tipo do local descoberto
    EV_SUPPLY = 17,       // a = 0 lançado, 1 coletado
    EV_ELITE = 18,        // a = tipo, b = mutações
    EV_STATUS = 19,       // a = 1 queimando, 2 sangrando, 3 veneno, 4 lentidão
    EV_MINIMAP = 20,      // a = centro x, b = centro z
    EV_NEW_KIND = 21,     // a = tipo de zumbi visto pela primeira vez na fase
    EV_ITEM = 22,         // a = item, b = quantidade, c = 1 se a mochila estava cheia
    EV_ITEM_USED = 23,    // a = item, b = 1 usado / 0 recusado (vida cheia etc.)
    EV_WEAPON = 24,       // a = arma equipada
    EV_WAVE = 25,         // a = onda, b = 1 se grande horda, c = 0 preparando, 1 começou, 2 vencida
    EV_CONTRACT = 40,     // a = 1 sem munição e sem reserva (gatilho vazio), 2 reserva no fim, 3 carga coletada (4D), 4 carga revelada no mapa (4D), 5 primeiro Inchado gerado por ninho (4C; b = ninho)
};

enum SoundId {
    SND_SHOT = 0, SND_RELOAD, SND_HIT, SND_HEADSHOT, SND_GROAN, SND_ZOMBIE_DIE, SND_HURT, SND_PICKUP,
    SND_PORTAL, SND_JUMP, SND_STEP, SND_EMPTY, SND_SPLASH, SND_ZOMBIE_ATTACK, SND_HEAL,
    SND_THROW, SND_FIRE, SND_EXPLOSION, SND_KNIFE, SND_SCREAM, SND_ROCK, SND_CHEST, SND_SPIT, SND_RACK,
    SND_SHOTGUN, SND_EQUIP, SND_WAVE,
    SND_COUNT
};

// Resultado da missão.
enum ResultIndex {
    R_ELAPSED = 0, R_HEALTH, R_MAX_HEALTH, R_KILLS, R_HEADSHOTS, R_CRYSTAL_MASK, R_DAMAGE_TAKEN,
    R_SHOTS, R_HITS, R_CHESTS, R_CHEST_DIAMONDS, R_DISTANCE, R_NESTS, R_WAVE, R_SCORE,
    // Contrato de expedição (etapa 4): bloco reservado 24..31.
    R_ALERTS = 24,        // zumbis alertados pelo jogador (sem ninhos/ondas)
    R_CARGO,              // 25 (4D) 1 = carga coletada
    R_RESERVE_PCT,        // 26 reserva de munição restante, centésimos de pente (-1 = ilimitada)
    R_BOMBS,              // 27 (4C) Inchados gerados por ninhos
    R_SIZE = 32
};

}  // namespace zz::proto
