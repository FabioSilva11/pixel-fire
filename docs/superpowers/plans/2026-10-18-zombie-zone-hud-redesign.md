# Zombie Zone HUD Redesign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Redesenhar o HUD de gameplay do Zombie Zone para melhorar hierarquia, legibilidade, espaçamento e ergonomia, sem alterar a cena 3D ou as funções dos controles.

**Architecture:** Manter `HudView` como renderer Canvas e preservar o protocolo do motor e o tratamento de toques. Ajustar o layout padrão em `HudLayout`, extrair o aviso transitório para um estado puro testável, e reorganizar os desenhos do HUD em `HudView`.

**Tech Stack:** Kotlin, Android Canvas/View, JUnit 4, Gradle.

**Spec:** Requisitos de redesign enviados pelo usuário nesta conversa; sem arquivo de especificação separado.

## Global Constraints

- Preservar cenário 3D, personagem, inimigos, perspectiva e dados da partida.
- Preservar todos os controles, áreas de toque, textos e significados existentes.
- Manter a interface em português e compatível com telas landscape de proporções diferentes.
- Manter layouts personalizados existentes; somente o padrão e os elementos visuais mudam.
- Não tocar no renderer nativo, na movimentação, no protocolo ou em funcionalidades fora do HUD.

## Review Focus

- Telas landscape estreitas (640 × 360 dp): controles e barra de itens devem caber com margem inferior mínima de 16 dp — teste em `HudLayoutTest.defaultLayoutFitsNarrowLandscape`.
- Modo canhoto: controles de combate devem continuar espelhados sem mover o minimapa — teste em `HudLayoutTest.defaultLayoutMirrorsCombatControlsForLeftHanded`.
- Layout personalizado antigo: coordenadas persistidas devem continuar sendo decodificadas — teste em `HudLayoutTest.decodePreservesCustomControlPositions`.
- Áreas de toque padrão: suprimentos, correr, joystick e controles de combate não devem se sobrepor — teste em `HudLayoutTest.defaultLayoutSeparatesTouchTargets`.
- Mensagens rápidas e combo: somente o aviso mais recente aparece e expira sem acumular — teste em `CombatFeedTest.latestNoticeReplacesPreviousAndExpires`.

---

### Task 1: Layout padrão responsivo

**Files:**
- Modify: `app/src/main/java/com/fabiosilva/ecosdeeldra/ui/HudLayout.kt`
- Create: `app/src/test/java/com/fabiosilva/ecosdeeldra/ui/HudLayoutTest.kt`

**Interfaces:**
- Consumes: `HudControl`, `ControlPos` e `HudLayout.defaults(wDp, hDp, leftHanded)`.
- Produces: dimensões e posições padrão responsivas para minimapa, joystick, controles, suprimentos e barra rápida; `decode()` continua preservando layouts personalizados.

- [ ] Escrever `defaultSizesMatchReferenceScale`, esperando diâmetros de 96 dp para o minimapa, 100 dp para o joystick, 80 dp para ATIRAR, 60 dp para MIRAR/PULAR/RECARREGAR e 54 dp para CORRER/FACA/GRANADA/AGACHAR.
- [ ] Escrever `defaultLayoutFitsNarrowLandscape`, `defaultLayoutSeparatesTouchTargets`, `defaultLayoutMirrorsCombatControlsForLeftHanded` e `decodePreservesCustomControlPositions`.
- [ ] Executar `.\gradlew.bat testDebugUnitTest --tests "com.fabiosilva.ecosdeeldra.ui.HudLayoutTest"`; confirmar falha nas expectativas novas.
- [ ] Ajustar os tamanhos base e as coordenadas do layout padrão para garantir controles dentro das margens, seis slots com pelo menos 16 dp de margem inferior e separação entre áreas de toque; preservar layouts salvos e interações existentes.
- [ ] Executar novamente `.\gradlew.bat testDebugUnitTest --tests "com.fabiosilva.ecosdeeldra.ui.HudLayoutTest"` e confirmar aprovação.

### Task 2: Avisos de combate sem acúmulo

**Files:**
- Create: `app/src/main/java/com/fabiosilva/ecosdeeldra/ui/CombatFeed.kt`
- Create: `app/src/test/java/com/fabiosilva/ecosdeeldra/ui/CombatFeedTest.kt`
- Modify: `app/src/main/java/com/fabiosilva/ecosdeeldra/ui/HudView.kt`

**Interfaces:**
- Consumes: `HudView.message(text, color)` e o valor `Hud.COMBO`.
- Produces: `CombatFeed.show(text: String, color: Int, nowMs: Long)` e `CombatFeed.current(nowMs: Long)`, mantendo uma única mensagem temporária e expirando-a após 1800 ms.

- [ ] Escrever o teste `latestNoticeReplacesPreviousAndExpires` para confirmar substituição e expiração.
- [ ] Executar o teste focado e confirmar falha porque `CombatFeed` ainda não existe.
- [ ] Implementar o estado mínimo de uma única mensagem e conectá-lo a `HudView.message`.
- [ ] Executar novamente `.\gradlew.bat testDebugUnitTest --tests "com.fabiosilva.ecosdeeldra.ui.CombatFeedTest"` e confirmar aprovação.

### Task 3: Reorganização visual do HUD

**Files:**
- Modify: `app/src/main/java/com/fabiosilva/ecosdeeldra/ui/HudView.kt`

**Interfaces:**
- Consumes: layout da Task 1 e aviso temporário da Task 2.
- Produces: barra superior segmentada com ninhos, diamantes, inimigos e cronômetro; minimapa/status compactos; bússola estreita; marcador “NINHO 276 m” sem duplicação de distância; aviso de combate único com combo secundário; controles, inventário de seis espaços e FPS em posições refinadas.

- [ ] Redesenhar o status e a barra superior sem sobreposição; exibir os quatro indicadores em uma única faixa e manter “40/120”, “0/3” e “MAPA” legíveis.
- [ ] Manter a bússola estreita abaixo da faixa superior; exibir “PRÓXIMO” como rótulo secundário e conservar “NINHO 276 m” junto ao marcador no mundo.
- [ ] Desenhar somente o aviso de combate mais recente acima do centro e manter “COMBO x2” em hierarquia secundária, sem cobrir a mira.
- [ ] Refinar os botões e seis slots do inventário; destacar a arma equipada e posicionar o FPS no canto inferior direito com margem segura.
- [ ] Preservar todos os papéis de toque e as funções existentes dos controles.
- [ ] Executar `.\gradlew.bat :app:assembleDebug :app:testDebugUnitTest`; confirmar APK gerado e suíte aprovada.
- [ ] Instalar o APK de depuração no aparelho Android Wi‑Fi já conectado, se disponível, e inspecionar uma nova captura de gameplay para conferir legibilidade, escala e ausência de sobreposições.

