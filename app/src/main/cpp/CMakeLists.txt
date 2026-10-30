cmake_minimum_required(VERSION 3.22.1)
project(zombiezone LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# PC de desenvolvimento com 4 GB de RAM: um clang por vez (o ninja usaria um por núcleo).
set_property(GLOBAL PROPERTY JOB_POOLS compile_pool=1 link_pool=1)
set(CMAKE_JOB_POOL_COMPILE compile_pool)
set(CMAKE_JOB_POOL_LINK link_pool)

add_library(zombiezone SHARED
    jni_bridge.cpp
    core/perf.cpp
    core/platform.cpp
    world/block_textures.cpp
    world/buildings.cpp
    world/island.cpp
    world/horizonte/block_mapping.cpp
    world/horizonte/terrain.cpp
    world/horizonte/terrain_noise.cpp
    world/horizonte/chunk_generator.cpp
    world/horizonte/village_generator.cpp
    world/horizonte_adapter.cpp
    world/fluids.cpp
    world/destruction.cpp
    world/structures.cpp
    world/terrain.cpp
    world/world.cpp
    render/blocky.cpp
    render/voxel_humanoid.cpp
    render/character.cpp
    render/gl_util.cpp
    render/renderer.cpp
    render/voxel_weapons.cpp
    game/game.cpp
    game/game_visual.cpp
    world/competitive_map.cpp
)

# Preserva a geração determinística do terreno a partir da semente.
target_compile_options(zombiezone PRIVATE -Wall -Wextra -Wno-unused-parameter -O2 -ffp-contract=off)

target_link_libraries(zombiezone
    android
    log
    GLESv3
)
