# Zombie Zone — Battle Royale

Versão `0.10.0` (versionCode 13).

Battle Royale online em mundo voxel, com servidor dedicado autoritativo e modos Solo, Duo e Squad. O projeto reaproveita o motor C++, câmera, personagens, animações, colisão e renderização OpenGL ES do jogo Android existente.

O Android envia comandos. A VPS valida movimento, tiros, dano, eliminações, coleta, inventário, equipes, zona segura e resultados. A identidade usa uma chave P-256 do Android Keystore; a comunicação HTTPS/WSS verifica o certificado do servidor. Nenhuma credencial administrativa faz parte do aplicativo.

O mapa Terra Escarlate mede 1.440 × 1.440 metros, tem 13 regiões e 52 construções com coordenadas e rotações fixas. Todos os blocos e as malhas são carregados antes da confirmação de entrada na partida. Modelos mais simples à distância e descarte do que está fora da câmera reduzem o trabalho de renderização. O mapa instalado no cliente é o mesmo pacote utilizado pelo servidor; somente o estado dinâmico é transmitido durante a partida.

O lobby permite escolher a formação, criar ou entrar em grupo por código, completar equipe e buscar partida. A partida inclui avião, salto, paraquedas, equipamentos coletados no terreno, zona segura, suprimentos, cobertura temporária de gelo, derrubada/reanimação em equipe e classificação. O celular não hospeda partidas. A tela ARMAS do lobby é uma prévia local; o equipamento da partida é concedido exclusivamente pelo servidor.

A meta de 40–50 participantes depende de testes de carga e da capacidade da VPS. A configuração conservadora inicial admite uma sala com 8 jogadores. Esse limite não constitui uma medição de capacidade nem garantia de desempenho. Ainda é necessário validar visual, controles e uso de memória em aparelho Android; o aparelho está indisponível e nenhum emulador é necessário para compilar.

## Compilação com pouca memória

Instale Android SDK 36, NDK 28.2.13676358, CMake 3.22.1 e JDK 17. Configure `ANDROID_HOME` ou o caminho do SDK no Android Studio. O projeto já limita Gradle a 512 MB, um trabalhador, Kotlin no mesmo processo e um compilador C++ por vez.

```powershell
.\gradlew.bat :app:testDebugUnitTest :app:assembleDebug --no-daemon --max-workers=1 --console=plain
```

O APK fica em `app/build/outputs/apk/debug/app-debug.apk`. A compilação inclui ARM64 e ARMv7. O mapa deve estar em `app/src/main/assets/terra-escarlate.zzmap`, acompanhado de `map-manifest.json`.

## Servidor Linux

Fontes, testes e implantação ficam em `server/`. O núcleo C++ é compilado em um módulo N-API para Node.js 24. As configurações são fornecidas pelo ambiente: `ZZ_MAP_FILE`, `ZZ_MAX_PLAYERS`, `ZZ_API_PORT`, `ZZ_GAME_PORT`, `ZZ_BIND_HOST`, `ZZ_TLS_CERT`, `ZZ_TLS_KEY`, `ZZ_DATA_FILE`. A chave TLS e qualquer segredo permanecem fora do repositório e do APK.

```sh
cmake -S server/native -B server/native/build -DCMAKE_BUILD_TYPE=Release
cmake --build server/native/build -j1
server/native/build/bake_map terra-escarlate.zzmap map-manifest.json
ZZ_MAP_FILE="$PWD/terra-escarlate.zzmap" ctest --test-dir server/native/build -j1 --output-on-failure
cd server
npm ci --ignore-scripts
npm test
```

O serviço systemd reinicia após falha e reinicialização da máquina; os resultados públicos são gravados de forma atômica. Antes de atualizar, valide a versão em staging, preserve o mapa, os binários e os dados atuais e mantenha uma cópia para reversão. O endpoint `/health` informa versão do protocolo/mapa, hash, salas e memória. O serviço aguarda o encerramento controlado das partidas ao receber sinal de atualização.

A análise, o projeto aprovado e o plano de execução estão em `docs/battle-royale/` e `docs/superpowers/plans/2026-10-29-battle-royale.md`.
