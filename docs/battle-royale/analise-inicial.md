# Análise inicial — transformação em Battle Royale

Data: 29/10/2026. Base: versão 0.9.0, commit 879ca0d.
Status: levantamento técnico atualizado após acesso SSH por senha; nenhuma funcionalidade nova implementada no cliente e nenhum serviço remoto alterado.

## Objetivo recebido

Transformar o jogo existente em Battle Royale online autoritativo, sem recomeçar o motor. Retirar zumbis, ondas, ninhos, cristais e portal. Preservar movimento, câmera, colisão, renderização voxel, animação, equipamentos, controles e configurações. Suportar Solo, Duo e Squad, matchmaking dedicado, avião/salto/paraquedas, zona segura, loot, coberturas temporárias, airdrops e resultados. Meta de 40–50 jogadores sujeita a medição real.

## O que o código atual permite reutilizar

| Sistema | Evidência | Adaptação necessária |
|---|---|---|
| Movimento | `game/game.cpp`, `Game::updatePlayer`: andar, correr, agachar, pular, nadar, degraus e escalada | Extrair estado e passo de simulação independente de câmera, áudio, animação e GL; usar a mesma regra no servidor e na predição |
| Câmera | `Game::updateCamera`, colisão de câmera por raycast | Permanecer apenas no Android; tiro validado deve usar origem autorizada e orientação dentro dos limites |
| Colisão | `world/world.cpp`, `moveBody`, `World::raycast`, `World::lineOfSight` | Compartilhar biblioteca de colisão e mapa canônico com o Linux |
| Combate | `updateWeapon`, `traceShot`, `fireShot`, projéteis e granadas | Separar intenção de disparo dos efeitos; servidor decide munição, cadência, alcance, obstrução, acerto, dano e eliminação |
| Inventário | `game_items.cpp`, `items.h`, `data/Items.kt`, `InventoryScreen.kt` | Migrar autoridade para o servidor; identificadores únicos para spawns/caixas e transações atômicas/idempotentes de coleta |
| Renderização | `render/renderer.cpp`, malhas opacas/recorte/água por chunk, corte por distância e seis planos do frustum | Preservar no Android; adicionar atores remotos, avião, paraquedas, gelo e interfaces do BR |
| Animações/modelos | `render/character.*`, `voxel_humanoid.*`, `voxel_weapons.*`, assets existentes | Manter locomoção/animações compatíveis e criar visuais originais para os novos objetos |
| Mundo | `World` já distingue mundo finito e streaming; terreno e estruturas existentes | Produzir um pacote fixo versionado; substituir escolha aleatória de objetivos/locais por composição explícita |
| UI Android | lobby, carregamento, HUD, mochila, ajustes e JNI existentes | Adicionar seletor Solo/Duo/Squad, equipe, fila e contagem no lobby, estado da conexão e resultados autoritativos |
| Diagnóstico | `core/perf.*`, testes nativos, testes JVM e limites de compilação de 512 MB/um worker | Reaproveitar e acrescentar medições de rede, simulação e carga |

A rede multiplayer anterior não existe mais no código ativo. O antigo PvP LAN não era um servidor dedicado competitivo; não deve ser reintroduzido como caminho de hospedagem no celular.

## Acoplamentos a resolver antes de um servidor Linux

`Game` contém `Renderer`, recursos de personagens e estado de simulação na mesma classe. O CMake atual produz uma biblioteca Android ligada a `android`, `log` e `GLESv3`. `core/platform.cpp` depende de JNI/AssetManager; `core/log.h` inclui `android/log.h`.

Não basta compilar o Game atual na VPS. A fronteira necessária é uma biblioteca C++ compartilhada de simulação/colisão/dados, sem dependências de UI/GL/JNI, com adaptadores de plataforma separados. O Android mantém seu renderer e envia entradas; o Linux executa o estado real da partida.

O movimento atual usa dt de quadro e altera animações/sons junto à física. Predição e reconciliação precisam de comandos numerados, simulação com passo definido e replay de entradas não confirmadas. Não se deve presumir determinismo exato entre ARM e x86: o estado autoritativo deve corrigir divergências.

O resgate atual para fora de buracos e o retorno ao início quando Y é muito baixo são decisões locais. No BR, qualquer reposicionamento precisa de decisão e validação do servidor.

## Remoção dos sistemas atuais

A retirada deve rastrear dependências, não somente ocultar elementos da UI:

- IA, navegação exclusiva dos inimigos, mutações, spawner, drops de inimigos e ondas;
- objetivos, núcleos de ninhos, cristais, portal e proteção de terreno específica dos objetivos;
- eventos, índices de HUD, resultados, áudio exclusivo e testes específicos desses sistemas;
- contratos e resultados da missão antiga que ainda estão no motor;
- inventário local como fonte de equipamento em partidas competitivas: o save local não pode conceder itens ao entrar em uma partida.

Projéteis, explosões, efeitos, estruturas, materiais e colisão hoje presentes em arquivos de missão/zumbis precisam ser extraídos quando compartilhados. Excluir esses arquivos cegamente removeria funcionalidades reutilizáveis.

## Mapa fixo e memória

O mapa atual tem limite de 6144 unidades e altura de 128, gerado por chunks de 16×16 sob demanda. Não é um mundo ilimitado, mas sua geração/composição é procedural e a área residente acompanha o jogador.

Para dimensionamento inicial, considerar provisoriamente uma unidade como um metro e manter altura de 128 blocos; confirmar escala antes da autoria definitiva. Em 1440×1440:

- 90×90 = 8100 chunks;
- 265.420.800 bytes de blocos de um byte = 253,125 MiB somente para o volume voxel;
- ground maps, metadados, estruturas, malhas CPU, VBOs, texturas, jogadores e cópias temporárias aumentam esse consumo;
- desenhar o mapa todo por quadro é diferente de possuir um mapa fixo instalado.

Há duas interpretações a decidir: mapa fixo com chunks internos/culling/residência limitada, ou todo o volume carregado antes da partida mantendo culling. Desenhar todos os chunks simultaneamente contraria a meta de celulares modestos e exige teste específico; não há evidência atual de viabilidade.

As 13 regiões solicitadas devem usar um manifesto canônico com ID, nome, limites, construção, X/Z, altura de apoio, rotação, material e revisão. Cliente e servidor devem validar o mesmo hash de conteúdo. Coordenadas/layout ainda não foram definidos neste levantamento. Terreno e construções serão próprios, reaproveitando os construtores existentes quando compatíveis, sem reproduzir layouts ou assets de outro jogo.

## Lacunas novas, não presentes no jogo atual

- Transporte autenticado, conexão e reconexão, números de sequência, controle de repetição e limites de entrada;
- API de sessões, identidade, equipes/convites, fila, matchmaking e alocação de partidas;
- representação e interpolação de jogadores remotos;
- validação de movimento/tiros e histórico limitado de poses para compensação de latência;
- estados de espera, avião, queda livre, paraquedas, aterrissagem, combate, derrubada, reanimação, eliminação e encerramento;
- safe zone e dano exclusivamente no servidor;
- calibres, capacidade/peso, capacetes/mochilas, armas adicionais e gelo com colisão autoritativa;
- airdrops compartilhados e loot sem duplicação;
- interest management: omitir atores dinâmicos desnecessários, não apenas esconder no renderer;
- resultados persistentes, observabilidade, scripts de implantação e operação.

## Transporte: pesquisa inicial

Candidato a avaliar, não decisão aprovada: GameNetworkingSockets. O projeto oficial oferece mensagens UDP confiáveis/não confiáveis e criptografia. A viabilidade da distribuição aberta no Android ainda exige verificação: o README menciona implementações mobile cujo código não é distribuído nesse repositório. Não adotar a biblioteca antes de confirmar esse ponto com um build NDK reproduzível. Isso não substitui autenticação da identidade da aplicação, tickets de acesso à partida, validação de comandos ou proteção da API.

Fontes primárias consultadas:
- https://github.com/ValveSoftware/GameNetworkingSockets
- https://github.com/ValveSoftware/GameNetworkingSockets/blob/master/BUILDING.md

Evitar criar criptografia própria. Protocolo de aplicação versionado, limites de tamanho/frequência, tickets curtos vinculados à sessão/partida, sequência de entradas e identificadores idempotentes continuam necessários independentemente do transporte escolhido.

## VPS: situação verificada

Destino recebido: 187.102.244.208, SSH 22, usuário de bootstrap root. Não há senha ou token neste documento.

Inspeção autenticada somente de leitura realizada em 29/10/2026:

| Recurso | Resultado observado |
|---|---|
| Sistema | Ubuntu 24.04 LTS, Linux 6.8.0-31, x86_64/KVM |
| CPU | 1 vCPU Xeon Silver 4314 |
| RAM | 960 MiB totais, aproximadamente 554 MiB disponíveis na amostra |
| Swap | ausente |
| Disco | volume virtual de 25 GB, aproximadamente 21 GB livres; o dispositivo anuncia ROTA=1, portanto SSD físico não confirmado |
| Portas | 22/TCP, 8080/TCP e 27015/TCP em escuta; não foi encontrada escuta UDP na amostra |
| Serviço do jogo | zombie-zone.service, usuário zombie-zone, diretório /opt/zombie-zone/source/server |
| Runtime | Node 24.22.1 com módulo nativo C++ |
| Memória do serviço | aproximadamente 97 MiB, sem salas no instante da amostra |
| Firewall local | iptables/ip6tables com políticas ACCEPT; estado do firewall do provedor não conhecido |
| Contêineres | Docker não encontrado pelo inventário de comandos |

O serviço existente oferece HTTPS e WebSocket com TLS, identidade por desafio P-256, equipes, matchmaking e módulo nativo BattleRoyale. A fonte contém classes de zona, avião, voo, loot, paredes, granadas e inventário. Isso é candidato a reaproveitamento, não evidência de que os requisitos completos estão corretos.

O health HTTPS foi validado usando o certificado público obtido via SSH com host conhecido, sem desligar a verificação TLS. Retornou protocolo 1, mapVersion 1, 13 regiões, mapa 1440, maxPlayers 50, zero salas, tick 20 Hz e snapshots 10 Hz. O limite configurado de 50 NÃO comprova capacidade.

Foi encontrada divergência: a fonte lida anuncia mapVersion 2, mas o processo ativo respondeu mapVersion 1. Antes de atualizar, identificar binário, mapa e fonte realmente usados, registrar hashes e preparar rollback. Não sobrescrever o serviço atual durante essa investigação.

A fonte atual do mapa remoto usa armazenamento comprimido com cache de 256 chunks descomprimidos. Isso não satisfaz sozinho a decisão de carregar todo o mapa antes da partida. Os nomes das regiões já aparecem na fonte, mas a composição arquitetônica original, coordenadas finais e rotações de cada construção ainda precisam de auditoria/autoria.

A rede foi inspecionada sem saturar o link; banda contratada, latência sob carga e capacidade de partidas continuam desconhecidas. Não foi executado benchmark de carga na VPS de 1 vCPU/960 MiB.

A senha foi usada somente no cliente SSH e não foi escrita em arquivos do projeto, relatórios ou configurações. Uma chave dedicada permanece disponível fora do repositório. Nenhuma instalação, abertura de porta, reinício ou remoção ocorreu.

## Validação exigida antes de afirmar capacidade

Primeiro estabelecer correção com poucos clientes. Depois executar cenários reais com 10, 20, 40 e 50 jogadores simulados, incluindo concentração numa região, disparos, loot simultâneo, gelo, zona final, perda/reordenação de pacotes e reconexão. Gerador de carga deve rodar fora da VPS sob teste e respeitar serviços existentes.

Medir distribuição do tempo de tick, CPU, RSS por partida, banda de entrada/saída, perdas, RTT, reconciliações e pausas. Medir também FPS, memória e aquecimento em Android durante voo e combate concentrado. Número de partidas simultâneas depende dos recursos medidos por partida e da reserva operacional; não é conhecido agora.

## Sequência candidata de entregas

1. Fechar decisões de mapa/transporte/identidade e obter inventário da VPS.
2. Extrair simulação C++ compartilhada e testar movimento/colisão sem GL.
3. Trocar missão por primeira partida dedicada Solo com poucos clientes, sem zumbis/objetivos antigos, com movimento e combate autoritativos.
4. Consolidar o mapa fixo e sua distribuição versionada; implementar ciclo de avião/salto e safe zone.
5. Acrescentar loot/inventário servidor, armas/equipamentos, gelo e airdrops.
6. Acrescentar equipes Duo/Squad, convites, marcações, derrubada/reanimação e reconexão.
7. Implantar no modo aprovado, medir carga/celulares e ajustar limites reais de capacidade.

Esta sequência é proposta para revisão; não representa funcionalidades já entregues nem reduz o escopo final solicitado.

## Decisões pendentes

- Decisão recebida: carregar o mapa inteiro antes da partida; usar LOD/HLOD e mipmaps à distância, mantendo chunks como estrutura interna e culling do que não é visível.
- Esclarecimento recebido: implementar o escopo do prompt completo; não presumir que “dois métodos” exige dois sistemas de implantação simultâneos.
- Acesso por senha já funcionou; uso de chave pode ser configurado posteriormente sem substituir acessos existentes.
- Domínio/TLS e modo de identidade/contas para publicação competitiva.

## Pesquisa de detalhe à distância

LOD reduz geometria; HLOD combina grupos distantes em proxies; mipmaps reduzem detalhe de texturas. O renderer existente já gera mipmaps para blocos e personagens, mas não contém seleção de malhas LOD/HLOD. Reaproveitar o mipmapping existente e acrescentar níveis de geometria/proxies. A colisão e a validação de tiros nunca devem usar a versão visual simplificada.

Carregar o mapa inteiro não significa desenhar cada objeto em todo quadro. Preparar o pacote inteiro antes de READY, com níveis de detalhe construídos previamente, memória compactada e orçamento medido. Os thresholds serão ajustados por testes de voo, movimento e combates.

Fontes primárias:
- https://dev.epicgames.com/documentation/unreal-engine/hierarchical-level-of-detail-overview-in-unreal-engine
- https://registry.khronos.org/OpenGL/specs/es/3.2/GLSL_ES_Specification_3.20.html
