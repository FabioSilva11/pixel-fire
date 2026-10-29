# Zombie Zone — Rifle híbrido TPS/FPS e ícone do app

## Objetivo

Adicionar ao jogo um rifle automático criado diretamente em Three.js. O rifle deve permanecer visível e preso ao personagem durante a exploração em terceira pessoa; ao segurar o botão `ATIRAR`, a câmera entra em primeira pessoa, o rifle passa para a composição da câmera e dispara automaticamente. Ao soltar o botão, o disparo para e a câmera retorna suavemente para terceira pessoa.

Também será criado um ícone Android próprio para Zombie Zone, preservando a identidade verde, laranja e escura do jogo.

## Referências e licenças

- `a-saeed/Blaster` será usado apenas como referência conceitual para hitscan, dispersão, cadência, munição, recarga e mira dinâmica. O repositório não contém uma licença publicada; nenhum código, asset ou expressão protegida será copiado.
- `Footprintarts/ThreeJS_FPS_2.0` possui licença MIT. Os sons `laser.mp3` e `reload.mp3` podem ser incorporados com o aviso MIT correspondente.
- O modelo `FpsRig.glb` não será incorporado. Ele é um rig de primeira pessoa creditado como CC-BY e não corresponde visualmente aos personagens atuais. A arma será construída proceduralmente em Three.js.
- O ícone será uma criação nova, sem reutilizar imagens dos repositórios de referência.

## Arquitetura

O sistema será dividido em unidades pequenas:

1. `rifle-model.js` construirá um `THREE.Group` low-poly com coronha, corpo, carregador, cano, mira e ponto do cano. A mesma instância será movida entre um suporte no personagem e um suporte na câmera.
2. `hybrid-aim.js` controlará o estado `third-person`, `entering-first-person`, `first-person` e `returning-third-person`, incluindo a interpolação da câmera e os offsets do rifle.
3. `automatic-fire.js` controlará pressão/soltura, cadência, dispersão determinística por disparo e solicitação de recarga automática.
4. `weapon-state.js` continuará responsável pela munição e recarga, ampliado para carregador de 30 tiros, recarga de 1,8 segundo e cadência automática.
5. `weapon-audio.js` tocará tiro e recarga com instâncias locais de `Audio`, respeitando `soundEnabled` e permitindo disparos consecutivos sem cortar o som anterior.
6. `main.js` apenas conectará entrada, câmera, rifle, hitscan, combate, HUD, efeitos e descarte.

## Comportamento da câmera e da arma

- Em terceira pessoa, o rifle será anexado a um `weaponSocket` filho do modelo do jogador. Como os GLBs atuais não oferecem ossos nomeados de forma confiável, o socket usará um offset proporcional à escala do personagem e acompanhará sua transformação sem modificar o modelo.
- `pointerdown` em `ATIRAR` captura o ponteiro, inicia a transição para primeira pessoa e marca o gatilho como pressionado.
- A transição dura aproximadamente 180 ms, reutiliza a câmera atual e preserva yaw/pitch. A câmera vai para a altura dos olhos, ligeiramente deslocada para evitar interseção com a cabeça.
- O primeiro disparo acontece quando a câmera chega ao limiar seguro de primeira pessoa. Enquanto o ponteiro permanecer pressionado, novos disparos seguem a cadência de 600 tiros por minuto (intervalo de 100 ms).
- `pointerup`, `pointercancel`, perda de foco, pausa, vitória, derrota ou descarte interrompem imediatamente o disparo e iniciam o retorno à terceira pessoa.
- Durante a primeira pessoa o corpo do jogador permanece no mundo, mas a cabeça e o rifle TPS não podem bloquear a câmera. O rifle é reanexado ao suporte FPS e o restante do personagem recebe ocultação somente para a câmera quando necessário, sem alterar colisões.
- O mesmo rifle volta ao socket TPS após a transição; nunca existirão duas armas visíveis simultaneamente.

## Disparo e dano

- O tiro será hitscan a partir do centro da câmera, com alcance de 45 metros.
- A dispersão começa pequena e cresce enquanto o gatilho permanece pressionado, retornando ao mínimo após soltar. O HUD refletirá isso abrindo e fechando a mira.
- Cada tiro válido consome uma munição, produz clarão do cano, recuo curto do rifle, som e um traço visual rápido.
- O raycast considera primeiro paredes e depois zumbis. Uma parede bloqueia o dano, mesmo que o alvo esteja alinhado atrás dela.
- Acerto no corpo causa o dano padrão do rifle. Uma região superior simples baseada na caixa do personagem recebe multiplicador de headshot, inspirada no conceito do Blaster sem copiar sua implementação.
- Ao zerar a munição, a recarga começa automaticamente. O botão `RECARREGAR` continua disponível para recarga antecipada.
- Durante a recarga o gatilho não dispara. O HUD mostra `.../30`, toca o som de recarga e volta a permitir tiro ao completar 1,8 segundo.

## Áudio

- `laser.mp3` será usado como som-base de tiro e `reload.mp3` como recarga, copiados do repositório MIT.
- O som de tiro usará um pequeno pool para sustentar a cadência automática.
- A opção existente `Som do jogo` controla música e efeitos. Desativá-la interrompe imediatamente sons ativos e impede novos efeitos.
- Falha de reprodução por política de autoplay não bloqueia o jogo nem a mecânica de tiro.

## Ícone Android

- O ícone será criado por geração de imagem como arte original: cabeça de zumbi low-poly verde em uma mira laranja, fundo azul-marinho quase preto, sem texto e com silhueta legível em tamanho pequeno.
- A arte será preparada como ícone adaptativo Android: fundo sólido separado e foreground centralizado com zona segura.
- Serão gerados recursos `mipmap-mdpi` até `mipmap-xxxhdpi`, além de `mipmap-anydpi-v26` para o ícone adaptativo.
- `AndroidManifest.xml` passará a usar `@mipmap/ic_launcher` e `@mipmap/ic_launcher_round`.

## Tratamento de falhas

- Se os sons falharem, o jogo continua silenciosamente e registra diagnóstico.
- Como o rifle é procedural, não há carregamento de GLB que possa deixar a arma preta ou ausente.
- A criação de materiais usa cores explícitas, `roughness` e `metalness` compatíveis com o render atual.
- Todo listener de ponteiro, blur e áudio será removido em `dispose` para não duplicar disparos ao reiniciar fases.
- O sistema cancela tiro e recarga visual ao pausar ou sair do gameplay.

## Testes e aceitação

- Testes unitários cobrirão cadência automática, pressão/soltura, munição, recarga manual/automática, dispersão e transições da câmera.
- Testes do rifle validarão a hierarquia procedural, o socket TPS, o suporte FPS e o ponto do cano.
- Testes de integração verificarão que paredes bloqueiam hitscan, zumbis recebem dano, headshots aplicam multiplicador e listeners são removidos.
- Testes de assets confirmarão sons, licença e todos os recursos de ícone.
- Validação no navegador confirmará: arma visível em TPS; segurar `ATIRAR` entra em FPS; disparo contínuo; soltar retorna; HUD e sons funcionam; console sem erros.
- A suíte existente deve permanecer verde e o APK deve terminar em `BUILD SUCCESSFUL` antes de qualquer instalação.

## Fora do escopo

- Multiplayer, replicação, server-side rewind e pickups do Blaster.
- Troca entre várias armas, shotgun, foguetes ou granadas.
- Alterações em mapas, colisões estruturais, progressão, personagens ou IA zumbi já adaptada.
