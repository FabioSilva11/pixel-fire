package com.fabiosilva.ecosdeeldra.ui

/**
 * Controles do HUD que o jogador pode mover, redimensionar e esconder (todos podem ser ocultados;
 * o joystick oculto continua funcionando como joystick invisível onde o polegar tocar).
 */
enum class HudControl(val key: String, val label: String, val optional: Boolean, val baseDp: Float) {
    // baseDp e o RAIO em dp na escala 1: o diametro desenhado e 2 * baseDp * ControlPos.scale
    // (os tamanhos do padrao vem da referencia medida, em HudLayout.REFERENCE).
    JOYSTICK("joy", "Movimento", true, 50f),
    FIRE("fire", "Atirar", true, 40f),
    FIRE_LEFT("fire2", "Atirar (2º botão)", true, 36f),
    AIM("aim", "Mirar", true, 30f),
    JUMP("jump", "Pular", true, 30f),
    RELOAD("reload", "Recarregar", true, 30f),
    CROUCH("crouch", "Agachar", true, 27f),
    SPRINT("sprint", "Correr", true, 27f),
    MELEE("melee", "Faca", true, 27f),
    GRENADE("grenade", "Granada", true, 27f),
    MEDKIT("medkit", "Kit médico", true, 27f),
    WEAPON("weaponpanel", "Painel da arma", true, 40f),
    EMOTE("emote", "Emoji", true, 22f),
    MAP("map", "Minimapa", true, 48f),
    PAUSE("pause", "Pausa", true, 20f),
    HOTBAR("hotbar", "Barra de itens", true, 22f),
    BAG("bag", "Mochila", true, 22f),
}

/** Centro do controle em frações da tela (0..1), escala e visibilidade. */
class ControlPos(var x: Float, var y: Float, var scale: Float = 1f, var visible: Boolean = true) {
    fun copy() = ControlPos(x, y, scale, visible)
}

class HudLayout(val controls: MutableMap<HudControl, ControlPos>) {
    operator fun get(c: HudControl): ControlPos = controls.getValue(c)

    fun copy() = HudLayout(controls.mapValuesTo(LinkedHashMap()) { it.value.copy() })

    fun encode(): String = "$VERSION;" + controls.entries.joinToString(";") { (c, p) ->
        "${c.key}=%.4f,%.4f,%.3f,%d".format(java.util.Locale.US, p.x, p.y, p.scale, if (p.visible) 1 else 0)
    }

    companion object {
        /** Versão da disposição: layouts salvos sem ela (anteriores ao raio X do HUD) são trocados uma vez pelo padrão. */
        const val VERSION = "v=2"

        const val MIN_SCALE = 0.6f
        const val MAX_SCALE = 1.8f

        /** Margem mínima entre a parte de baixo dos controles/barra de itens e a borda (a referência quase encosta). */
        const val BOTTOM_MARGIN_DP = 1f
        /** O joystick parado é um anel pequeno no canto: esta fração do raio de alcance. */
        const val JOYSTICK_IDLE = 0.5f
        /** Folga mínima entre a barra de itens e o botão de recarregar (dp). */
        private const val HOTBAR_GAP_DP = 12f
        // Tela da captura de referência (2712 x 1220 px a 3,0) em dp.
        private const val REF_W_DP = 904f
        private const val REF_H_DP = 406.6f
        private const val MIN_COMBAT_SCALE = 0.7f

        /**
         * Escala dos controles de combate: 1 na tela de referência e menor em landscape estreito, na mesma proporção
         * em que as distâncias (frações da tela) encolhem, para os botões não se encostarem.
         */
        fun combatScale(wDp: Float, hDp: Float): Float =
            minOf(wDp / REF_W_DP, hDp / REF_H_DP).coerceIn(MIN_COMBAT_SCALE, 1f)

        /**
         * Um controle da captura de referência (2000 x 900, medida em screenshots/xray-hud): centro em frações
         * da tela e raio em dp numa tela de 904 x 407 dp. [mirror] espelha no modo canhoto; [shrink] encolhe em
         * telas estreitas.
         */
        private class Spot(val c: HudControl, val fx: Float, val fy: Float, val rDp: Float, val mirror: Boolean = true, val shrink: Boolean = true)

        private val REFERENCE = listOf(
            Spot(HudControl.MAP, 0.0720f, 0.1472f, 56.6f, mirror = false, shrink = false),     // E01: quadrado de 113 dp
            Spot(HudControl.EMOTE, 0.2361f, 0.1174f, 18.3f, mirror = false, shrink = false),   // E03
            // E11: a engrenagem da referência (E08) fica colada no topo, onde o toque abre a cortina do sistema.
            Spot(HudControl.PAUSE, 0.7450f, 0.1444f, 12f, mirror = false, shrink = false),
            Spot(HudControl.WEAPON, 0.9178f, 0.1606f, 34.6f, mirror = false, shrink = false),  // E09 + E10: 138 dp de largura
            Spot(HudControl.JUMP, 0.9477f, 0.5606f, 46.1f),        // E13
            Spot(HudControl.FIRE, 0.8389f, 0.6847f, 37.8f),        // E14
            Spot(HudControl.CROUCH, 0.9466f, 0.8817f, 46.1f),      // E15
            Spot(HudControl.GRENADE, 0.7472f, 0.8978f, 26.8f),     // E17
            Spot(HudControl.MELEE, 0.7042f, 0.8242f, 19.4f),       // E18
            Spot(HudControl.RELOAD, 0.6980f, 0.9362f, 18.3f),      // E19
            Spot(HudControl.SPRINT, 0.1199f, 0.4667f, 45f),        // E20 (45,7 medido; encostava na mochila)
            Spot(HudControl.BAG, 0.0453f, 0.5163f, 24.7f),         // E21
            Spot(HudControl.MEDKIT, 0.0403f, 0.6647f, 24.1f),      // E22
            Spot(HudControl.AIM, 0.2745f, 0.5568f, 36.5f),         // E23
            Spot(HudControl.JOYSTICK, 0.0250f, 0.9214f, 45f),      // E24: anel parado de 22 dp, alcance de 45 dp
            Spot(HudControl.HOTBAR, 0.5580f, 0.8950f, 15f, shrink = false),  // E25: espaços de 30 dp (mínimo legível)
            Spot(HudControl.FIRE_LEFT, 0.3600f, 0.4000f, 37.8f),   // oculto por padrão
        )

        /** Metade da largura e da altura visíveis do controle de raio [r] (dp). */
        fun halfExtent(c: HudControl, r: Float): Pair<Float, Float> = when (c) {
            HudControl.HOTBAR -> r * 6f to r + 4f
            HudControl.WEAPON -> r * 2f to r * 1.3f
            HudControl.JOYSTICK -> r * JOYSTICK_IDLE to r * JOYSTICK_IDLE
            else -> r to r
        }

        /**
         * Layout padrão: a disposição da referência em frações da tela. Os controles de combate encolhem em telas
         * estreitas e tudo é mantido dentro da tela. Canhoto espelha os controles dos polegares; minimapa, emoji,
         * pausa, painel de armas e suprimentos ficam onde estão.
         */
        fun defaults(wDp: Float, hDp: Float, leftHanded: Boolean): HudLayout {
            val map = LinkedHashMap<HudControl, ControlPos>()
            val u = combatScale(wDp, hDp)
            val rawX = HashMap<HudControl, Float>()   // x em dp antes do espelho do modo canhoto
            for (s in REFERENCE) {
                val scale = s.rDp / s.c.baseDp * if (s.shrink) u else 1f
                val (hx, hy) = halfExtent(s.c, s.c.baseDp * scale)
                var x = s.fx * wDp
                if (s.c == HudControl.HOTBAR) {
                    // A barra não encolhe: termina antes do botão de recarregar, como a fileira de ícones da referência.
                    val reload = HudControl.RELOAD
                    x = minOf(x, rawX.getValue(reload) - reload.baseDp * map.getValue(reload).scale - HOTBAR_GAP_DP - hx)
                }
                x = x.coerceIn(hx, wDp - hx)
                rawX[s.c] = x
                val y = (s.fy * hDp).coerceIn(hy, hDp - BOTTOM_MARGIN_DP - hy)
                if (leftHanded && s.mirror) x = wDp - x
                map[s.c] = ControlPos(x / wDp, y / hDp, scale, s.c != HudControl.FIRE_LEFT)
            }
            return HudLayout(map)
        }

        /** Lê o layout salvo; controles ausentes (versões antigas) vêm do padrão. */
        fun decode(text: String, fallback: HudLayout): HudLayout {
            val result = fallback.copy()
            if (!text.startsWith("$VERSION;")) return result
            for (entry in text.removePrefix("$VERSION;").split(';')) {
                val parts = entry.split('=')
                if (parts.size != 2) continue
                val control = HudControl.entries.firstOrNull { it.key == parts[0] } ?: continue
                val v = parts[1].split(',')
                if (v.size < 4) continue
                val x = v[0].toFloatOrNull() ?: continue
                val y = v[1].toFloatOrNull() ?: continue
                val s = v[2].toFloatOrNull() ?: 1f
                result.controls[control] = ControlPos(
                    x.coerceIn(0f, 1f), y.coerceIn(0f, 1f), s.coerceIn(MIN_SCALE, MAX_SCALE),
                    v[3] != "0",
                )
            }
            return result
        }
    }
}
