# Zombie Zone

Jogo Android nativo em Kotlin e C++, com lobby 3D e ilha procedural.
Versão `0.9.0` (versionCode 12).

## Jogar

O lobby oferece **JOGAR**, **ARMAS** e **AJUSTES**. Jogar abre a tela de
carregamento e inicia uma nova ilha com o equipamento selecionado.

Objetivo: **destrua ninhos → recolha cristais → extraia pelo portal**.
Os três ninhos soltam cristais; recolher todos abre o portal. O mapa, os
zumbis, o combate, as construções e a geração do terreno continuam no motor
nativo. Os suprimentos encontrados durante a partida continuam disponíveis.

Em **ARMAS**, organize o equipamento e a mochila. Durante a partida,
a mochila pausa o jogo e devolve as alterações de equipamento ao motor.
Em **AJUSTES**, configure controles, HUD, som e gráficos.

## Dados locais

São gravados apenas equipamento e configurações.
O formato de save é 6. Saves anteriores preservam essas seções; a camada
de recuperação mantém uma cópia original antes da migração e oferece
restauração quando o arquivo principal está ilegível.

## Compilar com 4 GB de RAM

Use JDK 17 e Android SDK/NDK. O projeto limita o Gradle a 512 MB, usa GC
serial, um worker, execução sequencial e um compilador C++ por vez.

```powershell
.\gradlew.bat :app:testDebugUnitTest :app:assembleDebug --no-daemon --max-workers=1
```

O APK fica em `app/build/outputs/apk/debug/app-debug.apk` e contém ARM64 e ARM32.

```powershell
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

## Estrutura

- `app/src/main/java`: lobby, equipamento, configurações, HUD, áudio e save.
- `app/src/main/cpp`: motor de jogo, terreno, água, física, zumbis e renderização.
- `app/src/test`: testes de equipamento, controles, objetivo e recuperação.
- `tests/native`: verificações nativas do terreno e das interações do mundo.
- `scripts/test-terrain.ps1` e `scripts/test-world.ps1`: testes no Android conectado.
- `docs`: documentação técnica e registros históricos de desenvolvimento.
