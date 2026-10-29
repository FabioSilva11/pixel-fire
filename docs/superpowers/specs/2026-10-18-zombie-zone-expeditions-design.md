# Zombie Zone — Expedições de Sobrevivência Offline

## Intenção

Transformar o Zombie Zone em um jogo Android offline que gere partidas memoráveis,
decisões táticas e histórias que o jogador queira mostrar. O jogo não dependerá de
backend, servidor pago, ranking global ou conta. A meta é retenção por domínio,
descoberta e relatos de partidas — não por interrupções, moedas demais ou pressão de
anúncios.

O público deve entender o jogo em uma frase: **"prepare-se, explore uma ilha
infectada, assuma riscos por recompensas e extraia antes que a situação piore."**

## Restrições e decisões de produto

- Manter Android nativo, Kotlin + C++/NDK/OpenGL ES, orientação paisagem e o
  conteúdo atual de ilhas, zumbis, armas, itens, biomas e portal.
- Preservar o jogo inteiramente utilizável offline. Wi-Fi local continua opcional
  para PvP entre amigos, mas não é requisito de campanha nem de compartilhamento.
- Não transformar o jogo em sandbox voxel de mineração e construção livre. Esse
  caminho exige uma reestruturação grande e hoje não sustenta a promessa do produto.
- Não ampliar calendários, baús, moedas ou anúncios como mecanismo de retenção.
  A dificuldade nunca será definida por receita de anúncios.
- Construção, se introduzida, será tática e limitada: itens temporários que resolvem
  problemas de uma expedição, e não edição ilimitada do terreno.

## Direção escolhida

O produto passa a ser um **shooter de expedição e extração em ilhas voxel
procedurais**. Campanha, fases infinitas e sobrevivência passam a compartilhar a
mesma fantasia e os mesmos verbos:

```text
Escolher equipamento e contrato
        ↓
Explorar pontos de interesse, encontrar ameaças e suprimentos
        ↓
Destruir ninhos e decidir entre extrair ou correr mais riscos
        ↓
Extrair, receber um resumo narrativo e desbloquear novas opções táticas
```

A campanha continua oferecendo uma curva guiada de biomas e inimigos. As fases
procedurais deixam de parecer "conteúdo infinito vazio" ao receberem contratos,
modificadores e objetivos opcionais. Sobrevivência deixa de competir como um produto
separado: é uma variante de expedição cujo contrato privilegia resistir a ondas.

## Primeiro projeto: Fundação e expedição legível

Este é o primeiro incremento implementável. Ele corrige os problemas que impedem
qualquer conteúdo novo de ser percebido, sem exigir reescrever o motor inteiro.

### Experiência de primeira partida

1. A primeira fase começa com um tutorial jogável curto, sem tela de texto longa.
2. O jogador aprende, nesta ordem: mover, olhar, atirar, abrir o mapa, seguir um
   marcador, reconhecer o núcleo de um ninho, destruir o ninho, recolher o cristal
   e usar o portal.
3. A instrução persistente usa uma sequência única: **"destrua ninhos → recolha
   cristais → extraia pelo portal"**. Ela deve refletir exatamente as regras do
   motor.
4. Ao se aproximar de um ninho pela primeira vez, o jogo destaca o núcleo
   destrutível e explica a ação. Depois da destruição, destaca o cristal e, por fim,
   o portal.

### HUD e captura

- A mira e a área central da cena são reservadas para combate. Só o aviso de combate
  mais recente pode aparecer; ele expira rapidamente e nunca se empilha com outros.
- Objetivo, distância e bússola usam regiões distintas. A mesma distância não é
  mostrada duas vezes.
- O HUD existente continua personalizável, canhoto e acessível. Layouts salvos
  continuam válidos.
- Um futuro modo de captura separará visibilidade de controles e área de toque: o
  jogador pode ocultar os botões visualmente sem perder entradas. Ele será entregue
  junto do sistema de compartilhamento, não antes da clareza do HUD normal.

### Confiança e continuidade

- O save terá versão, cópia de segurança rotativa e recuperação explícita. Uma falha
  de JSON nunca poderá substituir silenciosamente o progresso por um jogo novo.
- Variantes debug usarão IDs de teste de anúncios; IDs reais ficam apenas em release.
- Documentação, versão e textos do jogo descrevem as regras que o motor realmente
  executa.
- O PvP LAN continua casual. O anfitrião deve validar remetente, cadência, munição,
  alcance e linha de tiro antes de registrar dano; ainda assim, ele não será vendido
  como competitivo ou como ranking global seguro.

### Desempenho

- Antes de crescer mapas ou adicionar mais inimigos, registrar tempo de frame,
  quantidade de chunks gerados e uploads de malha durante exploração.
- O streaming deixa de gerar e enviar trabalho ilimitado no mesmo frame. Trabalho de
  mundo será orçado por frame; geração pesada pode migrar para fila CPU quando a
  medição justificar a complexidade.
- O alvo inicial é estabilidade perceptível em aparelho Android intermediário, não
  apenas alta taxa em equipamento de desenvolvimento. A escala gráfica existente
  permanece como contingência, não como única solução.

## Segundo projeto: decisões de expedição

Depois de a primeira experiência estar clara e estável, cada saída recebe um
**contrato**. Um contrato combina uma recompensa explícita, uma regra de risco e um
objetivo opcional. Exemplos:

- **Recuperação silenciosa:** poucos inimigos, munição escassa, bônus por não alertar
  muitos zumbis.
- **Ninho instável:** ninhos geram explosivos; destruir todos concede um módulo raro.
- **Noite vermelha:** visão menor e inimigos mais agressivos; maior recompensa.
- **Caixa perdida:** localizar uma carga opcional antes da extração.

O contrato aparece antes da partida e não é monetizado. O jogador entende o motivo
de uma regra existir e escolhe seu risco.

Ninhos passam a funcionar como fontes claras de pressão: cada família de ninho tem
um comportamento comunicável por cor, som e inimigos que gera. O objetivo não é
somar tipos de inimigo, mas criar respostas diferentes: aproximar, recuar, gastar
explosivo, chamar distração ou extrair cedo.

## Terceiro projeto: profundidade tática sem sandbox total

O inventário atual recebe poucos recursos de sucata e material infectado. Em vez de
mineração livre, o jogador encontra esses recursos em baús, pontos de interesse,
ninhos e inimigos especiais. As primeiras receitas são deliberadamente pequenas:

- barricada dobrável: cria cobertura temporária;
- isca sonora: desloca uma horda;
- kit de campo: converte recursos em cura ou munição limitada.

Cada item deve resolver um problema de combate que já existe. Não será adicionada
uma receita que seja apenas mais uma barra de progresso. A construção limitada usa
posicionamento seguro e não modifica permanentemente o terreno procedural.

## Compartilhamento local e histórias de partida

O jogo gera uma ficha final local para cada expedição: seed, bioma, contrato, tempo,
abates, melhor combo, arma principal, eventos marcantes, itens raros, resultado e
causa da morte quando aplicável. Ela pode virar imagem para o compartilhamento nativo
do Android.

Seeds e contratos codificam desafios reproduzíveis. Amigos podem receber um código
ou QR, jogar a mesma ilha sob as mesmas regras e comparar manualmente seus cartões de
resultado. Isso cria conteúdo para vídeos e conversa sem servidor. Não haverá
alegação de placar mundial ou anti-cheat absoluto.

## Arquitetura de entrega

A reestruturação será incremental e guiada por limites claros, não por uma grande
reescrita.

- `MainActivity` deixa de concentrar fluxo de telas, resultados, anúncios, save e
  modos de jogo. Coordenadores pequenos assumem missão, progresso e PvP.
- `HudView` separa estado de mensagens, desenho e roteamento de toques. O plano de
  HUD já existente é incorporado como a primeira subetapa.
- `Game` passa a expor estados específicos de missão e HUD, evitando que novos
  contratos dependam de vetores implícitos sem documentação.
- Geração de mundo e sincronização de malhas recebem orçamento explícito por frame
  antes de qualquer tentativa de threading complexo.
- Cada regra pura de campanha, contrato, crafting e persistência terá testes unitários
  Kotlin. Regras nativas críticas receberão testes de integração ou cenários de
  diagnóstico reproduzíveis.

## Ordem de implementação

1. Completar a base de HUD em andamento e testar em tela landscape estreita.
2. Corrigir objetivo/tutorial/documentação, save recuperável e configuração de
   anúncios debug/release.
3. Adicionar diagnósticos de desempenho e limitar streaming por frame.
4. Criar o modelo de contratos e uma primeira variação de expedição com tela de
   preparação e resultado legível.
5. Criar ficha local de resultado e compartilhamento de imagem.
6. Adicionar os três recursos e deployables táticos.
7. Endurecer PvP LAN e extrair gradualmente os módulos que hoje estão em classes
   monolíticas.

Cada etapa precisa deixar o APK jogável por si só; nenhuma fase pode depender de
backend ou de conteúdo futuro para fazer sentido.

## Critérios de aceitação

- Um novo jogador conclui a primeira expedição sem consultar documentação externa.
- Durante combate, não há mais de um aviso temporário competindo com a mira.
- O objetivo em README, tutorial, HUD e motor é idêntico.
- Save inválido preserva recuperação e não zera progresso de forma silenciosa.
- Uma seed/contrato reproduzível e um cartão de resultado podem ser compartilhados
  sem serviço remoto.
- Um contrato altera decisões reais de exploração, combate ou extração e comunica seu
  custo e recompensa antes do início.
- O jogo continua compilando para arm64-v8a e armeabi-v7a, e testes relevantes passam
  antes de cada entrega.

## Fora de escopo deliberadamente

- Multiplayer online, conta, cloud save, ranking global ou backend.
- Construção livre, mineração de todos os blocos ou mundo persistente gigante.
- Compra de poder, dificuldade ligada a receita de anúncio e mais loops de login.
- Reescrever renderizador ou motor inteiro antes de métricas mostrarem necessidade.
