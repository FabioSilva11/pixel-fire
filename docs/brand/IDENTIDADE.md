# Zombie Zone — Identidade visual

Shooter de expedição e extração em ilhas voxel, público brasileiro. Tom tático, tenso, legível. Este guia vale para telas, HUD, cartão de resultado e ícone do app. Fonte de verdade da arte: `docs/brand/svg/`. Pré-visualização: `docs/brand/preview.html` (abrir no navegador).

## 1. Princípios

1. **Legibilidade antes de enfeite.** Tudo precisa ser lido em 1 segundo, com o polegar em cima, no sol. Forma antes de cor, cor antes de texto.
2. **Voxel, não pixel-sujo.** Formas feitas de blocos e cortes de 45 graus. Sem gradientes suaves nos ícones, sem sombras difusas. Relevo vem do degrau de bloco (como `BlockDrawable`).
3. **Um acento por tela.** O lime `#A8FF3E` é a marca e a ação principal. Vermelho é só perigo. Amarelo/ouro é só recompensa.
4. **Cor nunca sozinha.** Famílias de ninho e contratos têm forma própria além da cor (acessibilidade para daltonismo).
5. **Escasso é tenso.** Poucos elementos, bem contrastados. Cura, munição e recompensas são raras; a interface não grita por elas.
6. **Nada de anúncio na identidade.** Nenhum ícone ou cor sugere recompensa de anúncio dentro de contratos.

## 2. Paleta

Tokens em `ui/Ui.kt` (`Palette`). Os existentes não mudam; os novos são aditivos. Espelho em `res/values/colors.xml` (prefixo `zz_`).

| Token | Hex | Uso |
|---|---|---|
| `bg` | `#0A1120` | fundo de tela |
| `ink` (novo) | `#060B16` | fundo de cartão/selo, mais fundo que `bg` |
| `panel` / `panelSoft` | `#E6111A2E` / `#CC1A2440` | painéis |
| `line` / `lineStrong` (novo) | `#33A8FF3E` / `#66A8FF3E` | contorno de painel / destaque |
| `lime` / `limeDark` | `#A8FF3E` / `#5DA81E` | marca, ação principal, vida/OK |
| `text` / `muted` | `#F4F7FF` / `#9AA6C0` | texto |
| `gold` / `accent` | `#FFC83D` / `#FFC400` | recompensa, selo |
| `cyan` | `#3CE6FF` | informação, rádio, silêncio |
| `red` / `danger` (novo) | `#FF5A5A` / `#FF3D3D` | dano / perigo e Noite vermelha |
| `famHorde` (novo) | `#FF8A3D` | família horda |
| `famExplosive` (novo) | `#FFD23D` | família explosiva |
| `famToxic` (novo) | `#4FE3A0` | família tóxica |
| `contractSilent` / `Nest` / `RedNight` / `Crate` / `Endurance` (novos) | `#3CE6FF` / `#FF8A3D` / `#FF3D3D` / `#FFC83D` / `#A8FF3E` | cor de cada contrato |
| `resScrap` / `resInfected` (novos) | `#B8C4DA` / `#4FE3A0` | recursos |

Contraste: texto `text` sobre `bg`/`panel` passa de 12:1; `muted` sobre `bg` ~7:1. Não use `famExplosive` ou `gold` como cor de texto sobre fundo claro.

## 3. Tipografia

Somente fontes do sistema Android (já em `Fonts`): `title` = sans-serif-black (títulos, números grandes), `bold` = sans-serif-condensed bold (rótulos, chips, botões), `body` = sans-serif-medium (texto corrido). Rótulos em CAIXA ALTA com espaçamento de letras 0,05 a 0,1 em; texto corrido em frase normal. Mínimo 12 sp (rótulo), 14 sp (corpo). O wordmark é desenhado em formas (fonte pixel 5x7), nunca digitado.

## 4. Formas

- Cantos: raio 6 dp em botões e blocos, 10 dp em painéis. Cortes de 45 graus (chanfro) para marcadores táticos.
- Relevo de bloco: faixa clara em cima, 4 dp de sombra embaixo (`BlockDrawable`).
- Contorno fino `line`; destaque `lineStrong`. Sem sombras difusas.
- Marca: cubo isométrico (3 faces: lime, lime escuro, verde-musgo) com núcleo vermelho no topo e mira em cruz ao redor.

## 5. Tom de voz (pt-BR)

Curto, imperativo, tático, sem gíria forçada. Verbos de ação: PARTIR, EXTRAIR, RECOLHER, DESTRUIR. Números reais, sem promessa vaga ("+25% de recompensa", não "ganhe mais"). Erros dizem o que fazer. Nunca culpar o jogador na derrota: diga a causa ("Você caiu para a horda") e a próxima ação ("Tentar de novo"). Acentuação correta em tudo.

Exemplos: "Recuperação silenciosa: poucos inimigos, munição escassa." / "Código da ilha copiado." / "Ninho destruído. Faltam 2."

## 6. Regras de uso de ícones

- **Grade 24x24**, margem de 2 unidades; arte útil em 20x20.
- **Traço 2**, pontas e juntas redondas (`stroke-linecap/linejoin: round`); preenchimento sólido só em detalhes pequenos (olhos, núcleo, ponto).
- **Monocromático tintável**: arte branca (`#FFFFFFFF`) no VectorDrawable; a cor vem de `setTint`/`BrandIcons.draw(..., tint)`. Nunca embuta cor nos ícones 24x24.
- **Tamanhos mínimos**: 16 dp em linha de texto, 24 dp como padrão, 32 dp em cartão de contrato, 48 dp em destaque. Abaixo de 16 dp use só a forma da família (círculo/gota/bomba), sem texto.
- Área de toque mínima 48 dp mesmo com ícone de 24 dp.
- Ícone ao lado de texto: mesma cor do texto, ou cor do token da categoria (contrato, família).
- Marca colorida (wordmark, emblema, selo) **não** se tinge.
- Famílias de ninho: forma + cor juntas — horda = três círculos (laranja), explosivo = bomba com pavio (amarelo), tóxico = gota (verde-água).

## 7. Lista de ícones

Nome (SVG em `docs/brand/svg/<nome>.svg`) → drawable → chave Kotlin (`BrandIcons`).

| Grupo | Nome | Drawable | Chave |
|---|---|---|---|
| Contrato | recuperacao-silenciosa | `zz_ic_silent_recovery` | `RECUPERACAO_SILENCIOSA` |
| Contrato | ninho-instavel | `zz_ic_unstable_nest` | `NINHO_INSTAVEL` |
| Contrato | noite-vermelha | `zz_ic_red_night` | `NOITE_VERMELHA` |
| Contrato | caixa-perdida | `zz_ic_lost_crate` | `CAIXA_PERDIDA` |
| Contrato | resistencia | `zz_ic_endurance` | `RESISTENCIA` |
| Objetivo | ninho | `zz_ic_nest` | `NINHO` |
| Objetivo | cristal | `zz_ic_crystal` | `CRISTAL` |
| Objetivo | portal-extracao | `zz_ic_extraction_portal` | `PORTAL_EXTRACAO` |
| Família | familia-horda | `zz_ic_family_horde` | `FAMILIA_HORDA` |
| Família | familia-explosivo | `zz_ic_family_explosive` | `FAMILIA_EXPLOSIVO` |
| Família | familia-toxico | `zz_ic_family_toxic` | `FAMILIA_TOXICO` |
| Recurso | sucata | `zz_ic_scrap` | `SUCATA` |
| Recurso | material-infectado | `zz_ic_infected_material` | `MATERIAL_INFECTADO` |
| Implantável | barricada-dobravel | `zz_ic_barricade` | `BARRICADA_DOBRAVEL` |
| Implantável | isca-sonora | `zz_ic_sound_lure` | `ISCA_SONORA` |
| Implantável | kit-de-campo | `zz_ic_field_kit` | `KIT_DE_CAMPO` |
| Utilitário | compartilhar | `zz_ic_share` | `COMPARTILHAR` |
| Utilitário | codigo-qr | `zz_ic_qr` | `CODIGO_QR` |
| Utilitário | relatorio | `zz_ic_report` | `RELATORIO` |
| Utilitário | bussola | `zz_ic_compass` | `BUSSOLA` |
| Utilitário | caveira | `zz_ic_skull` | `CAVEIRA` |
| Utilitário | relogio | `zz_ic_clock` | `RELOGIO` |
| Utilitário | combo | `zz_ic_combo` | `COMBO` |
| Marca (colorido) | wordmark / logo / selo | `zz_wordmark` / `zz_logo` / `zz_seal` | `WORDMARK` / `LOGO` / `SELO` |

Mapa dos contratos: `ContractId` 1 SILENT_RECOVERY, 2 UNSTABLE_NEST, 3 RED_NIGHT, 4 LOST_CRATE, 5 ENDURANCE correspondem às chaves de mesmo nome acima. Famílias de ninho (0 horda, 1 explosivo, 2 tóxico): `BrandIcons.familyIcon(i)` e `familyColor(i)`. O doc 05 não define as famílias; esta é a proposta, a alinhar quando a etapa de ninhos fixar os ids.

## 8. Como usar no código

```kotlin
BrandIcons.draw(canvas, ctx, BrandIcons.NINHO, x, y, sizePx, Palette.contractNest)
val bmp = BrandIcons.toBitmap(ctx, BrandIcons.SELO, 256, tint = 0)   // tint 0 = cores da arte (marca)
```
Para marca colorida passe `tint` qualquer: `draw` só aplica tinta aos ícones 24x24 (`BrandIcons.icons`), nunca à arte (`BrandIcons.art`).

## 9. Fluxo de produção da arte

1. Edite/crie o ícone em `tools/brand_gen.py` (lista `ICONS`) e rode `python tools/brand_gen.py` (gera `docs/brand/svg/*.svg` e `preview.html`).
2. Rode `python tools/svg2vd.py --all docs/brand/svg app/src/main/res/drawable` (gera `zz_*.xml`).
3. Adicione a chave em `BrandIcons.kt`.

Conversor `tools/svg2vd.py`: aceita `path`, `rect` (com `rx`), `circle`, `polygon`, `polyline`, `line`, `g`; atributos `fill`, `stroke`, `stroke-width`, `stroke-linecap`, `stroke-linejoin`, `fill-rule`, `opacity`; cores `#RGB/#RRGGBB/#RRGGBBAA/none/currentColor`. Falha com erro explícito em `transform`, `style`, classes, gradientes, `use`, `defs`, `clipPath`, `mask`, `text`, filtros e tracejado. Texto vira forma antes.

## 10. Ícone do app (proposta, NÃO aplicada)

`docs/brand/svg/app-icon.svg` (108x108, fundo `#08111F`, emblema na zona segura de 66 dp). Os mipmaps atuais não foram tocados. Os PNGs 432x432 de proposta foram rasterizados com Pillow (polígonos, traços e arcos) em `docs/brand/launcher-proposta/` (`app-icon-432.png` com fundo e `logo-432.png` transparente); não foram conferidos num aparelho. Para aplicar, rode `scripts/generate-android-icons.ps1 -Source docs/brand/launcher-proposta/logo-432.png` (ou o app-icon) e só então revise os mipmaps.
