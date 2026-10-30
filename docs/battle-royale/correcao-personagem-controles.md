# Correção da regressão do personagem e dos controles — 30/10/2026

A referência é a main do GitHub, commit `879ca0d663ee7903c2b15876dd1a67a989d2371b`, confirmada por `git fetch origin main`. O port para Battle Royale havia reescrito o HUD e reduzido a seleção/aplicação das animações; essa simplificação não era necessária para a autoridade do servidor e contrariou a preservação do personagem e dos controles.

## Correções

- Recuperado o desenho original dos controles, painel dourado da arma, retículas, ícones, editor e menu local de emojis. O estado dinâmico e os slots continuam vindo do servidor.
- Recuperado o joystick dinâmico com base que acompanha o dedo, zona morta, controle simultâneo de disparo/câmera e arremesso ao soltar com cancelamento.
- Reconectadas as animações originais: direção do corpo conforme movimento, inclinação nas curvas, cadência proporcional à velocidade, corrida com arma nas costas, poses de mira parada e nado, salto sem repetição, recarga progressiva, troca e golpe de faca.
- Evitada a aplicação duplicada de poses procedurais sobre agachamento e salto que já têm animação própria. Cada jogador mantém suas próprias transições, recarga e coice.
- Recuperado o enquadramento original do lobby, com personagem sem arma e gestos; prévia armada restrita à tela de armas. O enquadramento dessa tela respeita a posição de seu painel.
- Mantidos o mapa fixo v4 sem montanhas flutuantes, as regras de Battle Royale e a validação autoritativa. Não foram reintroduzidos objetivos antigos, zumbis, missões ou recompensas.

## Verificação

- Regressão reproduzida no executável de testes no Android: movimento lateral mantinha o corpo preso à câmera (`expected 1.5708, got 0`).
- Após reconectar a lógica original, testes de animação passaram no Android. Foram adicionadas verificações de orientação, corrida/arma nas costas, mira parada, nado, agachamento, salto sem loop e morte.
- 70 testes JVM passaram, sem falhas ou erros, incluindo cinco regressões dos controles e adaptação dos IDs do inventário.
- A compilação e a validação visual da versão final são registradas em `output/validation/hero-hud-restoration-*.log`.

Compilação não comprova paridade visual de todos os gestos nem elimina a necessidade de testar as interações durante a partida online.
