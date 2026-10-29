# Água, destruição e ciclo do Horizonte

Pedido direto do usuário: importar o bloco de água e seu funcionamento do
Horizonte, fazer armas e granadas destruírem terreno e importar o ciclo de
dia/noite. Manter instalação no aparelho e configuração de compilação de 4 GB.

Abordagem: reutilizar os algoritmos do Horizonte, com adaptadores para o catálogo
e o mundo do Zombie Zone. Outra simulação aproximada não satisfaria a referência.

## Regras

- Água: fonte e oito estados de fluxo, quedas verticais, propagação lateral,
  desaparecimento de fluxo sem alimentação e formação de fonte entre duas
  fontes apoiadas. Mesmo tick de 0,25 s, fila limitada de 16.384 células,
  orçamento de 256 células/tick e varredura local a cada 2 s. Copiar texturas
  de água do projeto de origem; altura da superfície acompanha os níveis.
- Não acrescentar inventário, baldes, mineração, gravidade de areia ou lava
  dinâmica: esses são sistemas distintos. A água interage com o terreno
  destruído e ocupa os espaços livres. Conservar natação/extinção de fogo.
- Tiros: um bloco por impacto confirmado; granada do jogador: esfera com seu
  raio de explosão. Água, y=0, bedrock e portal/apoios essenciais dos ninhos
  não são destruídos. Dano aos objetivos continua em seu sistema próprio.
  Explosões de zumbis não passam a escavar por acidente.
- Alterações duram a partida e sobrevivem ao descarte/regeneração de chunks;
  uma nova partida começa com o terreno original. Atualizar chão auxiliar,
  colisão, navegação, malhas vizinhas, luzes e minimapas.
- PvP: host decide as quebras dos tiros após a validação existente. Clientes
  recebem alterações ordenadas com revisão e ACK cumulativo; retransmitir
  lotes até confirmação. A simulação de água é do host. Não acrescentar
  granadas ao inventário PvP, que atualmente só oferece fuzil.
- Dia/noite: 24.000 ticks a 20 Hz, ciclo de 1.200 s, com dia 0–600,
  entardecer 600–700, noite 700–1100 e amanhecer 1100–1200. Copiar luz/cores
  e direção solar de scene.cpp. Pausa congela o relógio da missão. PvP usa
  o tempo de partida do host; lobby mantém seu relógio de apresentação.
- Preservar a geometria da mesma semente em ARM32/ARM64: investigação mostrou
  FMA contraindo a expressão do fundo das ilhas em ARM64. Compilar com
  `-ffp-contract=off` e comparar ambos contra o original canônico sem FMA.

## Verificação

Testes nativos reais para água, níveis/queda/retração/fontes/limites, edição
persistente em streaming, chão/dirty/luzes, tiro e esfera de granada,
proteções e fases do relógio. Cobrir perda/duplicação de lotes de rede com
replay ordenado. Rodar fixtures originais em ambas as arquiteturas, compilar
Android em sequência, executar testes pertinentes e instalar com `install -r`.
Registrar falhas anteriores: quatro HudLayoutTest e lint ByteOrderMark em
SaveCodec.kt, arquivo confirmado idêntico ao HEAD. Nenhum dado deve ser apagado.
