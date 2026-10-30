# Proposta técnica — Zombie Zone Battle Royale

29/10/2026. Documento para revisão; não afirma implementação ou capacidade validada.

## Decisões e abordagem

Reaproveitar o motor Android local e avaliar/reutilizar o servidor existente na VPS. O usuário determinou mapa completo carregado antes da partida, com redução de detalhe à distância. A especificação integral continua sendo o escopo; Solo é a primeira entrega de validação, não substitui Duo/Squad.

Alternativas consideradas: recriar o jogo perderia movimento/renderização já prontos; restaurar o PvP LAN não atende autoridade dedicada; reutilizar cliente e servidor existentes preserva mais trabalho. Esta última é a proposta recomendada. O servidor remoto contém componentes compatíveis, mas ainda requer revisão funcional/de segurança e alinhamento de fonte/binário/mapa.

## Separação e reutilização

- Biblioteca C++ comum: mapa canônico, colisão, movimento, regras de armas, inventário e simulação da partida, sem Android/JNI/OpenGL. Extrair os trechos atuais em vez de copiar regras divergentes.
- Android C++: renderer, câmera, efeitos, animações, interpolação, predição e reconciliação. Kotlin: lobby, formação, fila/contagem, equipe, HUD, equipamento e resultados.
- Servidor dedicado: adaptar o núcleo BattleRoyale C++ já encontrado. O serviço Node existente pode manter sessões, equipes e matchmaking, com filas/limites explícitos. Não confiar em valores de dano, posição final ou itens vindos do cliente.
- O save local mantém configurações e apresentação. Equipamento competitivo nasce do estado do servidor; não importar a mochila offline como posse de itens em uma partida online.

## Rede, identidade e consistência

O serviço atual usa HTTPS/WSS, não UDP. Preservar a API e os componentes de identidade/equipe compatíveis após revisão. Para o fluxo competitivo, comparar WSS existente e transporte UDP protegido por biblioteca/protocolo estabelecido sob perda real; não afirmar que WSS oferece o mesmo comportamento de latência do UDP. Proposta inicial de menor mudança: manter HTTPS/WSS com pacotes binários, limites e backpressure e medir latência sob perda real; não trocar o serviço por UDP sem um ensaio que justifique a migração. A eventual troca exige build Android/Linux verificável e testes de interoperabilidade. Sem criptografia própria e sem transporte plaintext de tokens.

Identidade atual por desafio P-256 é candidata a manter; cliente prova posse de chave, recebe sessão curta e ticket vinculado à partida. Chaves privadas de identidade permanecem no Android Keystore quando suportado. Servidor valida expiração, consumo do desafio, entrada na partida, vínculo jogador/conexão e tentativas por origem. Segredos administrativos/SSH/TLS privados nunca entram no APK.

Comandos de entrada numerados e limitados; snapshots com tick, confirmação da última entrada e revisão do mapa. Movimento em passo fixo compartilhado. Predição local com replay das entradas ainda não confirmadas; interpolação dos remotos. RTT/jitter/perda e timeout visíveis no HUD. Snapshot inicial completo na reconexão, depois deltas. Comandos repetidos não reaplicam coleta, cura, uso, descarte ou gasto.

Começar medição com o tick atual de 20 Hz e snapshots de 10 Hz, sem tratar esses valores como meta definitiva. Limites de pacote, filas e backlog não podem crescer sem teto. Interest management filtra estado dinâmico por distância/relevância; dados privados de inventário não são enviados a adversários. O mapa não é retransmitido continuamente.

## Mapa completo e renderização à distância

Mapa finito de 1440×1440 metros e altura inicial de 128 blocos. Pacote único versionado com terreno, interiores, estruturas e manifesto. Mesma identidade de mapa no cliente/servidor, com hash criptográfico do pacote para integridade/versionamento. O cliente termina leitura e preparação de todo o mapa antes de READY; o servidor só inicia o voo quando os participantes elegíveis estiverem prontos, com timeout e tratamento de falha de carga.

Chunks continuam sendo unidades internas de organização, colisão e malha. Sem geração infinita, mudança de seed por cliente ou descoberta tardia de regiões durante a partida. Todo o volume de colisão deve estar expandido e residente antes de READY, sem descarte de chunks durante a partida. Malhas LOD/HLOD serão preparadas no bake e carregadas antes do início. Texturas/meshes podem permanecer em representações compactas apropriadas, mas nenhum setor do mapa será gerado ou buscado por rede/disco depois de READY. Se os testes mostrarem que a carga integral não cabe no dispositivo-alvo, apresentar a limitação ao usuário; não trocar silenciosamente por streaming.

- LOD próximo: superfícies detalhadas, portas, interiores e cobertura exata.
- LOD médio: geometria simplificada, preservando silhuetas e aberturas relevantes.
- HLOD distante: proxies de grupos de construções/terreno preparados no bake.
- Mipmaps: reaproveitar as cadeias já usadas no renderer.
- Frustum/distance culling: manter. Tudo carregado não significa desenhar tudo em cada quadro.
- Histerese nas trocas de LOD para evitar alternância frequente; thresholds definidos por perfil de qualidade e testes de voo/combate.
- Colisão, tiro e cobertura sempre usam o mapa canônico, independentemente do LOD visual. Testar frestas e alvos junto a aberturas para evitar paredes falsas ou cobertura invisível.

O volume bruto atual seria 253,125 MiB só de blocos. O orçamento precisa incluir malhas, GPU, entidades, texturas e picos de carga. A etapa de mapa só passa após medição real de carga completa e voo no Android modesto.

## Regiões e construções originais

`mapa-proposto.json` define as 13 regiões solicitadas e 52 âncoras de construção com X/Z e rotação explícitos. É uma composição inicial para revisão. Cada arquétipo precisa de geometria original e validação de acesso, cobertura, interiores e distribuição de loot; não basta renomear as construções procedurais atuais. A altura de apoio será fixada e gravada pelo bake. Não copiar layout, modelos, sons ou texturas de Free Fire.

## Estados da partida e modos

Fluxo: fila → sala de espera/contagem no lobby → validação de mapa/READY → avião → salto/queda livre → paraquedas → aterrissagem/combate → eliminação ou vitória → resultados. Estado e relógio autoritativos; falha no carregamento ou desconexão não pode travar a sala indefinidamente.

Solo: um jogador por equipe. Duo: dois; Squad: até quatro. Grupos por convite/código e preenchimento automático configurável; o líder controla entrada/cancelamento da fila. Não misturar formações. Duo/Squad incluem derrubada, sangramento, reanimação interrompida por distância/dano, marcações e reconhecimento de aliados. Resultado por equipe e jogador vem do servidor. Políticas de fogo amigo, tempo de sangramento/reanimação e duração de reconexão serão valores de configuração com defaults documentados e testes.

O avião segue rota sorteada pelo servidor. O salto manual e o salto forçado ao fim da rota são decisões validadas. Queda livre aceita direção limitada; abertura manual do paraquedas só em altitude válida, abertura automática protege a altitude mínima; servidor valida pouso e velocidade.

Safe zone com círculos contidos no círculo anterior e em área válida, aviso, interpolação do encolhimento, dano crescente e tempo autoritativo. HUD/minimapa mostram círculo atual, próximo e contagem. Sem respawn competitivo após eliminação.

## Combate, equipamento e loot

Preservar sensação de movimento/controles do jogo atual, extraindo regras de locomoção compartilhadas. Servidor decide velocidade, salto/escalada, colisão, água, queda e qualquer correção de posição. Remover resgate/teletransporte decidido apenas pelo cliente.

Validar origem/direção do tiro, arma equipada, cadência, recarga, munição, obstrução e alvo. Compensação de latência usa histórico curto e limitado de poses; nunca aceitar timestamp arbitrário para atingir um alvo do passado. Remover assistência de mira que usa zumbis e qualquer envio de dano pronto pelo cliente.

Armas: pistola, SMG, rifle, escopeta, precisão e corpo a corpo; munição por calibre; colete, capacete, mochilas com capacidade; cura e recuperação; granadas explosivas e fumaça; gelo temporário. Aproveitar modelos/regras existentes compatíveis, ampliar catálogo com IDs versionados, sem reaproveitar IDs antigos ambiguamente. Loot tem IDs únicos, alcance de coleta, capacidade e transações idempotentes. Múltiplos clientes não podem consumir o mesmo spawn.

Gelo valida posse, cooldown, distância, posição, interseção com jogador/mundo e limites por partida. Expiração/destruição e colisão são compartilhadas. Airdrops têm cronograma, avião, trajetória, queda/paraquedas, fumaça, marcador e conteúdo decididos pelo servidor. Não duplicar conteúdos entre coleta, morte, reconexão e encerramento.

## Remoção definitiva da missão antiga

Remover zumbis/IA/ondas, ninhos, cristais, portal, contratos e vitória da missão antiga, seus eventos/HUD, código exclusivo e assets exclusivos. Antes de excluir arquivos, extrair projéteis, granadas, dano, efeitos, colisão e construções compartilhadas. Não manter um jogo offline de missão disfarçado de matchmaking. Bots de teste, se usados, são explicitamente identificados e não contam como jogadores reais.

## Operação na VPS

Inventário confirmado: Ubuntu 24.04 x86_64, 1 vCPU, 960 MiB RAM, sem swap, volume virtual 25 GB; serviço atual ocupa TCP 8080/27015. SSD e banda contratada não confirmados. Health configurado para 50 não comprova suporte real.

Primeiro reconciliar fonte/binário/mapa: health informa mapVersion 1 e fonte lida usa 2. Guardar versão/artefatos anteriores e testar nova instalação em diretório isolado; não reiniciar o serviço atual para descobrir incompatibilidades.

Preferir systemd existente, usuário sem privilégios e diretórios próprios. Docker Compose será alternativa somente se necessário/solicitado e compatível com o inventário; não executar os dois sobre as mesmas portas. Variáveis de ambiente sem valores secretos versionados. Certificados/chaves restritos no servidor; API HTTPS, saúde pública com informação mínima, métricas/admin em loopback ou acesso restrito. Não sobrescrever firewall nem remover serviços alheios. Abertura de UDP somente após transporte definido e conferência do firewall do provedor.

Atualização com backup consistente de dados, release versionada, verificação de saúde e rollback. Durante atualização, parar novas alocações e drenar partidas dentro de limite; quedas inesperadas reiniciam o processo sem inventar resultados/vitórias. Logs sem tokens/senhas, rotação/limites e métricas de CPU/RSS/banda/tick/conexões/partidas. Não executar benchmark agressivo enquanto houver partidas reais.

## Entregas e critérios de aceite

1. Reconciliar o servidor existente e isolar a simulação reutilizável; teste de movimento/colisão em Linux e Android.
2. Cliente online Solo com poucos jogadores: conexão real, fila/contagem, comandos validados, remotos interpolados e resultado autoritativo. Remover missão antiga junto da substituição funcional.
3. Pacote fixo completo com as 13 regiões, READY, LOD/HLOD e medição real de RAM/carga/voo.
4. Avião, salto/paraquedas, safe zone, loot/armas/equipamentos, gelo e airdrops completos e sincronizados.
5. Duo/Squad, convites/preenchimento, marcações, derrubada/reanimação e reconexão.
6. Operação e testes reais de carga/perda/jitter/reordenação. Crescer de poucos clientes para 10/20/40/50 e concentração na zona final. Medir gerador fora da VPS. Liberar número de partidas simultâneas só com CPU/RSS/banda/tick e reserva operacional medidos.

As entregas dividem a implementação; todas as funcionalidades do prompt permanecem no escopo final. Não declarar 50 jogadores, produção segura, ausência de cheats ou todos os modos concluídos antes de evidência dos testes respectivos.

## Fontes da pesquisa

- Epic: HLOD/proxy meshes — https://dev.epicgames.com/documentation/unreal-engine/hierarchical-level-of-detail-overview-in-unreal-engine
- Khronos: mipmapping/LOD em OpenGL ES — https://registry.khronos.org/OpenGL/specs/es/3.2/GLSL_ES_Specification_3.20.html
- OpenSSL: DTLS e suporte Android são candidatos técnicos, não escolha implementada — https://docs.openssl.org/3.5/man3/DTLSv1_listen/ e https://github.com/openssl/openssl/blob/master/NOTES-ANDROID.md

## Estado de aprovação

Requisitos do prompt e a decisão de carga completa/LOD vieram do usuário. Esta proposta reúne as escolhas técnicas e os limites encontrados para aprovação antes da refatoração do jogo e de qualquer atualização do serviço existente.

## Defaults propostos pelo implementador após revisão

- Referência inicial de Android modesto: 4 GB de RAM, OpenGL ES 3.0 e meta de 30 FPS; manter minSdk 24. Essa é uma proposta de QA, não uma capacidade já demonstrada. Medir também no aparelho conectado; não usar teste em aparelho mais potente como prova de desempenho em 4 GB.
- Identidade inicial: convidado autenticado pela chave do aparelho, com ID estável enquanto a chave existir. Convites/equipes e resultados são vinculados a esse ID. Não adicionar login por e-mail, senha ou provedor externo sem novo requisito; reinstalação/troca de aparelho não oferece recuperação de conta nesta entrega. Documentar essa limitação na UI relevante.
- Reaproveitar systemd existente; não instalar Docker apenas para oferecer uma segunda forma de executar o mesmo serviço.

Revisão de planejamento concluída. Nenhuma ação remota de escrita foi executada.
