# Integração do terreno Horizonte — registro de 24/10/2026

> Registro histórico da integração. Os trechos sobre combate em rede e suas
> ferramentas de teste foram substituídos pela remoção desse módulo em
> 29/10/2026. Consulte o README para os comandos e recursos atuais.

O Zombie Zone usa o núcleo contínuo do Horizonte para relevo, ruído, rios,
três biomas, cavernas, minérios, árvores, aldeias e ilhas flutuantes.
Os arquivos e hashes de origem estão em `horizonte-terrain-source.json`.

O mundo de missão tem 6144 × 6144 × 128 blocos, água em y=16 e ilhas
procedurais com topos entre y=61 e y=82. A área inicial fixa especial do
Horizonte não é usada. A arena e o lobby também usam o núcleo importado.

Ninhos, portal, início e construções de missão são sobreposições locais em
áreas secas. Os objetivos continuam no chão inferior. Os blocos usam
equivalentes do catálogo local; lava é magma sólido e diamante subterrâneo
usa minério de ferro, sem adicionar mineração ou recompensas econômicas.

Os moldes de estruturas usam chaves de 64 bits para suportar y acima de 63.
Os vértices usam coordenadas locais ao chunk, com origem aplicada no shader,
evitando o transbordamento anterior em regiões além de 4096 blocos.
O lobby/protocolo PvP passou a ZZ3/version 3 para recusar versões sem o
journal de alterações de terreno.

## Água, impactos e relógio

A água preserva o algoritmo do Horizonte, suas texturas, fonte, sete níveis
laterais e queda vertical. Atualiza a cada 0,25 s, com fila limitada a 16.384
células e 256 células por tick. Chunks não carregados e limites do mundo
são barreiras para a simulação. O host simula a água de toda a arena PvP.

Tiros removem o bloco atingido; granadas escavam uma esfera com seu raio de
explosão. Água, fundo do mundo, bedrock, portal/obsidiana e suportes essenciais
dos ninhos são protegidos. Explosões de zumbis preservam o comportamento
anterior. As edições sobrevivem ao streaming dentro da partida e são
descartadas numa nova partida. Não são gravadas no progresso do jogador.

No PvP, somente tiros validados pelo host podem escavar. O journal usa lotes
ordenados, ACK cumulativo e retransmissão; uma reconexão recebe o histórico.
Água e crateras usam o mesmo transporte. O ciclo solar tem 1.200 s e usa as
cores, fases e direção solar originais; a pausa congela o relógio da missão.

## Verificação

- 40 chunks comparados com fixtures geradas pelo código original: passaram,
  incluindo semente zero, coordenadas negativas e bordas de chunks/células.
- Testes nativos no Android ARM64 e ARMv7: passaram para mapas, materiais, ilhas,
  objetivo/início seco, espaço livre, três ninhos/portal, sobrevivência,
  regeneração independente da ordem, arena, altura da malha/minimapa e chaves.
- Revisão independente encontrou altura máxima do chunk desatualizada no
  mundo finito. A regressão falhou antes da correção e passou depois,
  verificando também a malha elevada e o topo visível da arena.
- Representação das aldeias e da água no mapa corrigida e coberta por regressão.
- Contrato de precisão: `-ffp-contract=off`. A investigação independente
  reproduziu uma diferença de um bloco causada por FMA em quatro fixtures
  ARM64. O original canônico e o port agora concordam em 40/40 nas duas ABIs.
- Suíte de interação executada no aparelho antes da revisão: passou para
  persistência/reinício, fluxo/queda/retração/fontes, níveis/imersão/malha,
  tiro, esfera, proteções e fases do relógio. Regressões adicionais da revisão
  cobrem frestas de cachoeiras e invalidação de luzes da arena.
- Transporte nativo: testes de lotes, perda de ACK, duplicação, sobreposição,
  seed, limites e endpoint forjado, incluindo sockets UDP reais e reconexão,
  passaram durante a implementação. Script reprodutível: `scripts/test-network.ps1`.
- Suítes World/UDP com as regressões finais: compiladas em ARM64 e ARMv7,
  sem avisos, e executadas com sucesso no aparelho nas duas arquiteturas
  em 25/10/2026. A regressão da fresta de cachoeira falhou no executável
  anterior à correção e passou no corrigido; luzes/fogueiras da arena,
  fluxo, crateras, persistência, relógio e UDP real também passaram.
- Protocolo da sala Kotlin: 13 testes passaram na execução Gradle final.
- Codec do bot: dois testes passaram; a regressão de IDs de fluxo falhou
  antes da correção e passou depois. Comando: `node --test tests/lan-terrain-codec.test.mjs`.
- Revisão independente identificou e corrigiu a comparação de níveis na
  lateral da cachoeira, efeitos luminosos persistentes na arena e o limite
  antigo de IDs no robô de rede.
- C++ ARM64 e ARMv7, incluindo água/destruição/ciclo/rede e correções da
  revisão: compilados e vinculados com sucesso, um por vez.
- APK anterior do terreno: compilado e instalado no aparelho com `install -r`,
  preservando os dados. O pacote contém as bibliotecas das duas arquiteturas.
- Inspeção visual no aparelho: terreno, árvores, personagem, HUD e ilha
  flutuante renderizados. Missão da semente 42 iniciou com três ninhos e portal;
  carga registrada de aproximadamente 3,3 s. O diagnóstico foi encerrado e o
  aplicativo reaberto no modo normal.
- Amostra de memória da execução: aproximadamente 314 MB de PSS total,
  37 MB de heap nativo e cerca de 11 MB de buffers de chunks na GPU.
- Suíte JVM: 251 testes, 247 passaram e quatro falhas preexistentes em
  `HudLayoutTest`: `defaultLayoutFitsNarrowLandscape`,
  `defaultLayoutMirrorsCombatControlsForLeftHanded`,
  `defaultSizesMatchReferenceScale` e `defaultLayoutSeparatesTouchTargets`.
  A mesma situação foi registrada antes da integração (250 testes).
  A execução final de água/destruição/ciclo repetiu os 251 testes e as mesmas
  quatro falhas. O comando combinado com `--continue` concluiu o APK, mas
  devolveu erro por causa desses testes, sem falha de compilação.
- Análise Android lint: um erro preexistente `ByteOrderMark` em
  `data/save/SaveCodec.kt:132`, 206 avisos e um hint. Esse arquivo foi
  confirmado idêntico ao HEAD; não foi alterado nesta integração.

A conexão de depuração foi restabelecida em 25/10/2026, após o usuário
reativar o aparelho. Novo endpoint descoberto por mDNS: 192.168.1.231:39059.
As extensões de água/destruição/ciclo foram instaladas com `install -r`,
preservando os dados. O SHA-256 do APK instalado coincide com o artefato:
`352ae3a9d1e6d0c1917c1a0488e8e28ca01afab346745747134031bde4f2bb74`.
Uma missão da semente 42 abriu em aproximadamente 3,28 s; terreno, árvores,
HUD e ilha flutuante foram inspecionados na imagem do aparelho. Não houve
erro fatal nos logs capturados. O diagnóstico foi encerrado e o aplicativo
reaberto normalmente, sem os extras de teste.

O APK final de água/destruição/ciclo foi gerado com sucesso por uma execução
isolada de `:app:assembleDebug` (59 s). O pacote foi copiado para
`output/ZombieZone-Horizonte-debug.apk`; as bibliotecas ARM64/ARMv7 presentes
nele foram comparadas por SHA-256 com a saída atual do build e coincidiram.
As três texturas de água também estão no APK. Registro específico dessa
versão em `output/terrain/agua-apk-verification.json`, com `installed=true`.
Logs finais em `output/terrain/final-world-*.log` e `final-network-*.log`;
imagem em `output/terrain/agua-device-preview.png`.

Os logs, a imagem do aparelho e o registro SHA-256 do APK estão em
`output/terrain/`; o APK está em `output/ZombieZone-Horizonte-debug.apk`.

## Compilação com 4 GB de RAM

Configuração persistente em `gradle.properties`: heap de 512 MB, GC serial,
um worker, sem tarefas paralelas e Kotlin no mesmo processo. O CMake
mantém um compilador/linker por vez. Compilação, lint e testes devem ser
executados em sequência. A verificação inicial simultânea ficou sem memória;
os builds nativos e o empacotamento passaram após a execução sequencial.

As alterações locais anteriores foram preservadas. Não foram criados
commits, mudanças no projeto de origem, nem alterações nos dados do aparelho.
