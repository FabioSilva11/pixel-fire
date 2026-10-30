# Battle Royale Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Transformar o motor existente em cliente Android de Battle Royale dedicado, com carga integral do mapa e detalhe à distância.
**Architecture:** Reaproveitar o núcleo C++ do servidor existente, separar renderização/entrada de autoridade, integrar Kotlin HTTPS/WSS e substituir a missão. Manter o serviço atual até validação da release em staging.
**Tech Stack:** C++17, Kotlin/Android API24+, OpenGL ES3, Node24/WSS/TLS, systemd.
**Spec:** docs/battle-royale/proposta-tecnica.md (aprovada pelo usuário em 29/10/2026: “implemente”).

## Global Constraints

1440×1440 metros, 128 blocos; mapa inteiro carregado antes de READY; 13 regiões; Solo/Duo/Squad; nenhuma hospedagem no celular; nenhuma senha/chave privada versionada ou no APK. Compilar serialmente com heap512MB. Meta40–50 somente após testes reais. Não alterar serviços não relacionados nem executar ações destrutivas na VPS.

## Review Focus

Replay de comandos e coleta simultânea; reconexão sem inventar resultados; tiros atravessando gelo ou terreno; divergência de mapa e READY antecipado; OOM durante carga integral e voo.

## Execução e responsabilidades

- [x] Preservar base0.9.0 e criar branch battle-royale; obter snapshot de fonte remota sem segredos em output/vps-source-reference.
- [ ] Núcleo autoritativo: game/battle_royale.*, br_inventory.h, competitive_wire.h, mission_movement.h, world/body_motion.h. Manter snapshotv1 (cabeçalho24; jogador32; loot7; gelo10; drop7; fumaça5; inventário48), sequência/validação forte, coleta atômica, passos fixos, equipes/voo/zona. Adicionar regressões em tests/native/battle_royale_tests.cpp e competitive_wire_tests.cpp. Observar falha antes da correção; testar C++ serialmente em staging Linux/Android.
- [ ] Mapa/LOD: world/competitive_map.*, competitive_source.h, island.* e world/renderer conforme necessário. CompetitiveMap.load/save/copyChunk/blockAt/hash/regions/buildings permanecem interfaces. Aplicar manifesto de52 construções; adicionar preload completo e seleção LOD/HLOD mantendo colisão completa. Testar formato, limites, identidade, permanência de8100chunks e contagem/redução de malhas. Assets finais no APK com hash idêntico ao servidor.
- [ ] Backend: server/src/*.mjs, native/addon.cpp/CMakeLists.txt, deploy e testes. Corrigir READY, filas, reconexão, taxas, privacidade/interesse e persistência; nenhuma troca de portas/processos atuais. Manter protocolos HTTPS/WSS atuais como base, payload input52bytes e snapshotsbinários. Testar Node isoladamente sem levantar servidor nativo pesado em paralelo.
- [ ] Cliente C++/JNI: game/game.*, jni_bridge.cpp e CMake Android. Manter modelos, poses, câmera e renderer; trocar simulação antiga por snapshots remotos e predição local com regras comuns, replay e interpolação. Novas APIs prepareBattle(localId,look), applyBattleSnapshot(FloatArray), pollBattleInput():FloatArray e battleMapHash(). Remover IA/zumbis/objetivos/contratos/ondas e arquivos exclusivos depois de extrair código compartilhado.
- [ ] Cliente Kotlin: engine/BattleSession.kt (HTTPS/WSS autenticado/Keystore, reconexão/backpressure), BattleState.kt (snapshotvalidado), NativeBridge e MainActivity. UI Solo/Duo/Squad, grupos/convites, fila/contagem no lobby, carregamento/READY, HUD/ações de voo/loot/gelo/aliados, resultado. Não enviar mochila offline para o servidor. Testes de codec/estado e ciclo de conexão; preservar ajustes.
- [ ] Integrar e validar: testes JVM, nativos e backend; assembleDebug ARM64/ARM32 com1worker; instalar e exercitar no Android conectado. Comparar hash do mapa, SSL, entrada/saída/reconexão e memória. Revisão independente da branch; corrigir achados importantes.
- [ ] Implantação: snapshot dos artefatos/mapa/DB atuais, staging com limites CPU/RAM e portas temporárias em loopback, health e testes; atualização controlada do serviço relacionado, rollback pronto, persistência/backups/rotação. Medir carga fora da VPS e documentar capacidade real. Não declarar desempenho ou50jogadores sem evidência.

## Ledger

29/10: usuário aprovou execução. Ruling: trabalhar em branch separada na pasta atual para não duplicar builds/caches no PC4GB. Ruling: importar apenas arquivos necessários da fonte VPS; não restaurar código antigo da missão ou LAN. Serviço remoto continua intocado durante implementação local. Fonte e mapa ativos possuem divergência v1/v2 e devem ser reconciliados no staging.

30/10: execução avançada: cliente C++/JNI e Kotlin online integrados; IA/objetivos/contratos/ondas e suas telas removidos. Reaproveitados câmera, esqueleto/IK, animações, renderização e movimento. Servidor Node/WSS/TLS com barreira READY, grupos, reconexão, inventário e resultados autoritativos. Revisão independente encontrou avanço de sequência em WAITING, fechamento WSS sem resposta e sala encerrada sem retorno ao lobby; os três caminhos foram corrigidos.

Validação intermediária:65 testes JVM,23 testes Node/TLS/nativo e5 testes C++ passaram. Compilação ARM64/ARMv7 serial com512MB. Usuário inicialmente dispensou teste físico e depois liberou o aparelho; ADB atual192.168.1.231:38369 (Redmi, Android16, cerca8GB RAM). Instalado0.10.0/code13. Um crash do lobby revelou amostragem de pose sem bind; corrigido por sampleBind, com regressão nativa observada antes e depois. Lobby e carga integral/mapa online foram exercitados no aparelho; pico intermediário PSS~1.16GB no mapa3, portanto não afirmar adequação a aparelhos4GB.

VPS: release piloto /opt/zombie-zone/releases/20261030-br-0.10.0 ativa, mapa3 hash1383872027, cap8/uma sala, TLS verificado. Snapshot de reversão /var/backups/zombie-zone/deploy-20261030T041227Z. Firewall dedicado preserva SSH22/TCP8080/TCP27015 e não modifica outras tabelas; verificados SSH novo e HTTPS após ativação. Backups diários, rotação e métricas próprias instalados; primeiro backup executado. Nenhum segredo em fontes/APK.

Nova orientação explícita: remover montanhas/ilhas flutuantes. Geração aérea foi excluída do Horizonte e dos adaptadores; novo formato de conteúdo mapa4 para impedir mistura cliente/servidor. Pacote4 gerado em staging com hash1678534476; testes adicionais de terreno/pose (8 no total) em validação. O ensaio de roundtrip inicialmente excedeu a RAM da VPS com serviço ativo; loader agora reutiliza colisão completa imutável apenas após validar conteúdo/metadados idênticos. Não houve troca do serviço público para mapa4 ainda. Próximos passos: concluir8 testes +23 TLS, baixar assets4, compilar APK com correções de clipping do lobby/câmera do avião, implantar release mapa4 com backup, instalar e repetir salto/movimento/loot/resultados no aparelho. Não encerrar como concluído antes dessas verificações.
