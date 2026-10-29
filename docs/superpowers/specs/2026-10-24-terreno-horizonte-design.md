# Terreno do Horizonte no Zombie Zone

Estado: aprovado pelo usuário com “continue”; implementação em verificação.

Revisão independente: sem perguntas importantes em aberto para planejar
a implementação.

## Pedido e escolha do usuário

Usar o mesmo sistema de geração de terreno do projeto local
`C:/Users/kirit/Desktop/horizonte`, incluindo seu ruído. Na escolha de escopo,
o usuário pediu **terreno completo, incluindo ilhas flutuantes**.

## Abordagens

1. Importar o gerador completo, adaptando os blocos e os objetivos do Zombie
   Zone. Traz relevo, rios, biomas, cavernas, minérios, árvores, aldeias e ilhas
   flutuantes. Exige altura de 128 blocos e integração com os objetivos.
2. Trocar apenas o ruído e o relevo, mantendo a ilha atual. Mudança menor,
   mas não traz o sistema completo solicitado.

O usuário escolheu a primeira abordagem. A implementação deve reaproveitar
as fórmulas e constantes reais do Horizonte, e não aproximar seu aspecto
com outro ruído.

## Origem e fidelidade

Fonte: `horizonte/app/src/main/cpp/horizonte/terrain_noise.{h,cpp}`,
`terrain.cpp`, `terrain_grid.h`, `chunk_generator.cpp` e
`village_generator.cpp`.

Usar o caminho contínuo `Terrain::chunk`, que funciona para qualquer chunk.
O cenário inicial especial `Terrain::initial`, suas oito ilhas com posições
fixas e a aldeia histórica de `elevated_terrain.cpp` não substituem esse
gerador contínuo. As ilhas procedurais por células de 128 blocos de
`chunk_generator.cpp` estão incluídas.

Preservar o hash de 64 bits, interpolação suave, precisão double, frequências,
amplitudes, regras de rio, biomas, cavernas e distribuição das ilhas. Não
substituir globalmente o RNG usado por combate, contratos ou itens.
Preservar a geração independente da ordem de carregamento dos chunks.

## Integração proposta

- Campanha e sobrevivência usam o terreno contínuo importado dentro do
  limite horizontal existente de 6144 por 6144 blocos. Remover o recorte
  radial da ilha antiga nessa geração; a fronteira finita do jogo permanece.
- A altura passa de 64 para 128 blocos; nível da água acompanha o Horizonte
  (bloco de água até y=16). Distinguir topo sólido e primeira posição livre
  para evitar deslocamentos de um bloco.
- Árvores e aldeias do gerador importado são incluídas. Converter os IDs do
  Horizonte para blocos equivalentes do catálogo local. Objetos decorativos
  sem equivalente usam um bloco existente coerente, sem importar o catálogo
  inteiro nem mecânicas de construção/mineração.
- Minérios existentes usam os blocos locais. Diamante subterrâneo recebe um
  equivalente visual disponível, sem criar recompensas econômicas novas.
  A lava do terreno usa o magma existente: não é introduzida uma simulação
  de líquidos ou dano nova.
- Os três biomas naturais seguem a classificação espacial do Horizonte.
  A ambientação, o céu e a dificuldade da fase continuam nos sistemas de
  contrato/fase do Zombie Zone; não devem alterar a geometria para uma
  mesma semente. A Terra Devastada pode continuar como ambientação da fase.
- Ninhos, portal, início, baús e construções de missão continuam como
  sobreposições do Zombie Zone. Selecionar chão seco e espaço livre,
  sem sobrepor aldeias ou volumes das ilhas elevadas. Nivelar apenas a
  região necessária para cada estrutura e conservar conexões transitáveis.
- A navegação e os spawns continuam referenciando o chão inferior. Não
  tratar o topo de uma ilha flutuante como chão da coluna inteira. A colisão
  voxel permite que o personagem caminhe sob as ilhas e sobre seus blocos.
- Mapa geral e minimapa devem representar o novo terreno, incluindo a
  projeção das ilhas elevadas, com os marcadores de missão legíveis.
- A arena PvP usa o mesmo núcleo de terreno e ruído, com limite e áreas
  seguras de arena. A decoração fixa do lobby pode conservar sua composição;
  qualquer relevo procedural usado nela deve chamar o mesmo núcleo.
- Como o PvP gera o mapa em cada aparelho, atualizar a identificação de
  compatibilidade de terreno no lobby/protocolo para impedir partidas entre
  clientes com geradores diferentes. Manter os espelhos e o jogador-robô
  coerentes com essa identificação.

## Ajustes técnicos obrigatórios

O armazenamento atual das estruturas empacota x/z de 13 bits e y de 6 bits
em uint32_t. A nova altura exige 7 bits para y e, portanto, uma chave de
64 bits com empacotamento e leitura consistentes. Testar coordenadas altas
para impedir colisões entre posições.

Adaptar o preenchimento de colunas, consultas de altura e mapa ao terreno
importado. As estruturas atravessando chunks precisam ser recortadas
deterministicamente e consideradas nas células vizinhas que intersectam
seus limites. Não manter o gerador antigo oculto como fonte do mapa.

O projeto contém alterações locais anteriores em combate, itens, interface,
documentação e áudio. Preservá-las e limitar a edição às integrações
necessárias para o terreno. Não modificar o projeto Horizonte de origem.

## Verificação e critérios de aceite

1. Comparar amostras de ruído, altura, rio, bioma, cavernas e volumes
   flutuantes contra o código de origem, com sementes diferentes, inclusive
   zero, e coordenadas em bordas de chunks/células.
2. Verificar blocos equivalentes de chunks antes das sobreposições locais,
   incluindo cavernas, água, árvores, aldeias e ilhas flutuantes.
3. Verificar igualdade ao regenerar chunks em ordens diferentes e ausência
   de recortes nas bordas de árvores, aldeias e ilhas.
4. Verificar início seco com espaço para o jogador, três ninhos distintos,
   portal válido e estruturas abaixo da altura máxima para várias sementes.
5. Verificar que a chave das estruturas distingue posições com y acima de
   63, inclusive x/z próximos do limite do mundo.
6. Compilar Android ARM64 e ARMv7 e executar os testes existentes pertinentes.
   Fazer inspeção visual no dispositivo/emulador se disponível; registrar
   explicitamente qualquer verificação visual que não puder ser feita.

## Decisões deixadas à implementação

Organização dos módulos e nomes, equivalências decorativas de blocos,
cache de amostras e instrumentação dos testes. Evitar alocar o mundo inteiro
ou executar o gerador de aldeias novamente para cada bloco do mapa.
A duplicação da altura aumenta a memória dos blocos residentes; medir e
conservar o carregamento por chunks e os limites de memória existentes.

## Fora do escopo

Importar inventário, fabricação, mineração, persistência de mundos,
catálogo de 16 bits, mobs ou interface do Horizonte; oferecer mundo infinito;
adicionar voo ou acesso automático às ilhas. A fidelidade do terreno não
implica importar essas mecânicas.
