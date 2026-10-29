# Auditoria do personagem — 28/10/2026

Escopo autorizado: comparar a página de testes e o personagem nativo, usar os FBX de Downloads, pesquisar melhorias e publicar o resultado verificado na main. Trinta agentes em etapas (limite simultâneo: três auxiliares).

## Comparação e implementação (15 agentes)

| Agente | Foco | Evidência e decisão |
|---|---|---|
| 01 | Geometria | Torso, cabeça, cabelo e pernas coincidem. Polegares e ombros diferem. Evitar mudança de rig sem comparação visual controlada. |
| 02 | Animações | Idle recebe velocidade .6; fades interrompidos partem do alvo anterior. Priorizar continuidade com regressão. |
| 03 | Armas | Troca de attachment em peso .5 pode causar salto; mão da recarga mantém pegada do guarda-mão. Ampliar observabilidade antes de retocar contatos. |
| 04 | Retarget | Mudança T-pose/braços para baixo é intencional. Suspeita inicial de wrap no endpoint foi refutada por teste e fonte do Three.js. Problema confirmado: duração fracionária pode perder último trecho por arredondamento. |
| 05 | Materiais | Paleta e normais planas já coincidem. Luzes da página são fixas; nativas seguem câmera. Não restaurar pipeline cinematográfico. |
| 06 | IK | Alcance limitado sem reposicionar alvo e vetores paralelos são riscos numéricos. Sem reprodução confirmada nas poses atuais. |
| 07 | Downloads | Cinco FBX, todos integrados: Standing Aim Idle 01 (3.767s), Standing Death Left 01 (2.267s), Mutant Walking (1.433s), Bboy Hip Hop Move (2.200s), Swimming (4.533s). Nenhum arco encontrado. |
| 08 | Página | Faltam disparo/recarga, pausa e tempo determinístico. Capturas atuais avançam tempo arbitrário. Adicionar controles e API reproduzível. |
| 09 | ZZA | Formato corresponde ao conversor. Parser não rejeita FPS infinito/transformações não finitas; assets atuais confiáveis. |
| 10 | Continuidade | Snapshot da pose exibida ao interromper fades. Regressão falhou antes (2.75 vs 10.25) e passou no Android; índice negativo também protegido. |
| 11 | Página | Disparo único, recarga, pausa, avanço/scrub e diagnóstico. Câmera determinística implementada e verificada em navegador. |
| 12 | Conversão | Amostra final exata; cinco FBX regenerados, só dance mudou (diferença máxima .00002291 grau). Endurecimento de precisão, sem evidência de glitch visual corrigido. |
| 13 | Revisão animação | Sem problemas substanciais; casos normais e interrupções cobertos. |
| 14 | Revisão página | Encontrou câmera ainda avançando com pausa e falta de teste real de navegador; correção solicitada. |
| 15 | Revisão conversor | Sem bloqueios; duração de playback continua quantizada, teste com FBX ausente pula explicitamente. |

## Pesquisa online (15 agentes)

| Agente | Fontes primárias e resultado |
|---|---|
| 01 — mixer | [Three.js r180 AnimationMixer](https://raw.githubusercontent.com/mrdoob/three.js/r180/src/animation/AnimationMixer.js), [AnimationAction](https://raw.githubusercontent.com/mrdoob/three.js/r180/src/animation/AnimationAction.js). update(0) mantém tempo atribuído; confirma correção de endpoint sem alegar wrap. |
| 02 — blend | [ozz blending](https://guillaumeblanc.github.io/ozz-animation/samples/blend/), [inertialization do autor](https://theorangeduck.com/page/inertialization-transition-cost). Snapshot preserva pose, não velocidade; manter correção pequena, inertialização fica para evidência futura. |
| 03 — IK | [ozz IK](https://guillaumeblanc.github.io/ozz-animation/documentation/ik/), [solver oficial](https://github.com/guillaumeblanc/ozz-animation/blob/master/src/animation/runtime/ik_two_bone_job.cc). Confirma raio interno/externo e riscos de singularidade; não alterar solver sem varredura visual de poses. |
| 04 — contatos | [Epic Hand IK](https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-blueprint-hand-ik-retargeting-in-unreal-engine), [Unity Two Bone IK](https://docs.unity3d.com/Packages/com.unity.animation.rigging@1.2/manual/constraints/TwoBoneIKConstraint.html). Frame compartilhado de arma é apropriado; limiar de holster continua candidato a correção posterior com contato contínuo. |
| 05 — normais | [Khronos transformação de normais](https://github.com/KhronosGroup/Vulkan-Tutorial/blob/main/en/Advanced_glTF/Skeletal_Compute_Skinning/02_skinning_math.adoc#handling-normals), [Three material](https://threejs.org/docs/pages/MeshStandardMaterial.html). Matriz atual basta para escala uniforme; preservar facetas, sem adicionar PBR/cinemático. |
| 06 — desempenho | [Android game performance](https://developer.android.com/games/optimize/gameperformance), [Arm draw calls](https://learn.arm.com/learning-paths/mobile-graphics-and-gaming/analyze_a_frame_with_frame_advisor/analyze/). Medir CPU/GPU e partDraws antes de batching; 67 ossos não significam 67 desenhos. Nenhum ganho de FPS alegado. |
| 07 — root motion | [Unity root motion](https://docs.unity3d.com/6000.0/Documentation/Manual/RootMotion.html), [ozz extração](https://guillaumeblanc.github.io/ozz-animation/samples/motion_extraction/). Encontrou alias do primeiro vetor na remoção XZ. Regressão falhou antes e passou após clonar endpoints. Cinco FBX regenerados; Y/rotações preservados nesta correção, morte idêntica. |
| 08 — locomoção | [ozz blend](https://guillaumeblanc.github.io/ozz-animation/samples/blend/), [Unity Blend Trees](https://docs.unity.com/en-us/engine/6000.0/manual/animation-section/animation-mecanim/animation-animator-controller/animation-state-machines/animation-blend-trees/class-blend-tree). Idle estava a .6/.5; restaurado playback1 para idle local/remoto. Sincronização de passos exige alinhamento de contato antes de implementar. |
| 09 — capturas | [Playwright clock](https://playwright.dev/docs/clock), [comparação visual](https://playwright.dev/docs/test-snapshots), [OrbitControls](https://threejs.org/docs/pages/OrbitControls.html). Fixar viewport/DPR/câmera; pixels comparados apenas no mesmo navegador/execução. Teste real adicionado. |
| 10 — validação | [Khronos glTF](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#floating-point-data), [validator](https://github.com/KhronosGroup/glTF-Validator). Assets atuais finitos/normalizados verificados; parser de formato próprio ainda candidato a hardening para entrada corrompida. Não alegar defeito atual dos assets. |
| 11 — recarga | [Unity IK](https://docs.unity3d.com/Packages/com.unity.animation.rigging@1.3/manual/constraints/TwoBoneIKConstraint.html), [Epic Blend Nodes](https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-blueprint-blend-nodes-in-unreal-engine). Orientação de palma/dedos por fase é próxima melhoria estética, dependente de contatos medidos; página agora permite inspecioná-la. |
| 12 — cor | [Three renderer](https://threejs.org/docs/pages/WebGLRenderer.html), [color management](https://threejs.org/manual/pages/color-management.html). Exposição não garante paridade sem mesmas luzes/curva; manter preset fixo e não alterar renderer por suposição. |
| 13 — tempo | [Android frame pacing](https://developer.android.com/games/sdk/frame-pacing), [Three Clock](https://threejs.org/docs/pages/Clock.html). Preservar limite dt; página agora rebaseia relógio ao mudar visibilidade e usa interpolação de câmera por dt. Não alegar frame pacing medido. |
| 14 — acesso | [W3C slider](https://www.w3.org/WAI/ARIA/apg/patterns/slider/), [W3C button](https://www.w3.org/WAI/ARIA/apg/patterns/button/). Estados aria-pressed adicionados aos toggles fixos; botão Pausar/Continuar usa texto de ação sem aria-pressed; recarga tem valor textual português. |
| 15 — revisão integrada | [Unity root motion](https://docs.unity.com/en-us/engine/6000.7/manual/animation-section/animation-mecanim/avatar-creationand-setup/root-motion). Sem problemas substanciais no diff; remoções preservadas. Correção linear não equivale a foot locking. |

## Melhorias e limites

- Continuidade de fades interrompidos, velocidade authored de idle, conversão horizontal sem mutar referência e endpoint exato.
- Página com disparo/recarga manuais, pausa, quadro, tempo, scrub e capture absoluto. Viewport e câmera controlados no teste real de navegador.
- Não foram alterados rig, dimensões de membros, materiais nem solver IK por recomendações ainda sem validação visual suficiente. Não foram incluídas bibliotecas de renderização ou um pipeline cinematográfico.
- Os cinco arquivos originais de Downloads já eram utilizados; a melhoria é na conversão e validação. Não há novo clip de arco verificado.

## Como reproduzir

1. `npm ci --prefix tools/anim-converter` e `node --test tests/*.test.mjs`.
2. `./scripts/test-character.ps1 -Serial <adb-serial>` para as regressões nativas no Android.
3. `npm install --prefix output/browser-qa --no-audit --no-fund playwright` e `node scripts/test-character-preview.cjs` (Edge ou Chrome instalado, rede para o CDN Three.js).
4. `./gradlew.bat assembleDebug testDebugUnitTest`.

## Verificação final das melhorias

- Build final bem-sucedido para arm64-v8a e armeabi-v7a (8m9s).
- 12 testes Node passaram sem skips; regressões nativas de transição passaram no Android arm64. Os 247 testes JVM permaneceram válidos/up-to-date no build final.
- Navegador real: controles acionados, transformações da arma/mão verificadas, repetição de capturas com histórico diferente e pixels estáveis durante pausa. Nova execução isolada passou após ajustes de tempo/acessibilidade; incluiu mudança de vista durante pausa.
- Um ensaio simultâneo de navegador e compilação falhou por falta de memória; repetição isolada passou. Sem alegação de teste bem-sucedido para aquele ensaio.
- APK final instalado com Success e processo iniciado. Logs de crash consultados vazios. Não houve validação visual completa da jogabilidade, pois o celular estava em Dozing.

## Verificação da remoção

- Commit `0cdffce` publicado na main após instalação.
- APK para arm64-v8a e armeabi-v7a compilado.
- 247 testes JVM: zero falhas/erros; dois testes Node de terreno passaram.
- Instalação Wi-Fi retornou Success. Processo do aplicativo presente e nenhum crash nos logs consultados.
- Celular estava em Dozing; captura preta não comprova validação visual. Não foi solicitado desbloqueio ao usuário.

## Critério de implementação

Corrigir defeitos reproduzíveis e melhorar a comparação com mudanças pequenas. Pesquisa não é justificativa para incluir toda técnica encontrada. Manter saves, rig voxel, OpenGL restaurado, disparo manual e remoção integral de Cofre/cinemático/tiro automático.
