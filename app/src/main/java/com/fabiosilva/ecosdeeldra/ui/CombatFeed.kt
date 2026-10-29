package com.fabiosilva.ecosdeeldra.ui

/**
 * Aviso temporário de combate: guarda UMA única mensagem. Uma nova mensagem substitui a anterior
 * (nada se empilha sobre a mira) e ela expira sozinha após [DURATION_MS].
 */
class CombatFeed {
    class Notice(val text: String, val color: Int, val shownAtMs: Long, val priority: Int = PRIORITY_NORMAL)

    private var notice: Notice? = null

    /**
     * Mostra um aviso. Um aviso de prioridade menor não apaga um mais importante que acabou de
     * aparecer (menos de [MIN_SHOW_MS]); depois disso, qualquer aviso novo substitui o atual.
     */
    fun show(text: String, color: Int, nowMs: Long, priority: Int = PRIORITY_NORMAL) {
        val cur = notice
        if (cur != null && priority < cur.priority && nowMs - cur.shownAtMs < MIN_SHOW_MS && nowMs - cur.shownAtMs < DURATION_MS) return
        notice = Notice(text, color, nowMs, priority)
    }

    /** A mensagem ainda válida em [nowMs], ou null se não há nenhuma ou ela já expirou. */
    fun current(nowMs: Long): Notice? {
        val n = notice ?: return null
        if (nowMs - n.shownAtMs >= DURATION_MS) {
            notice = null
            return null
        }
        return n
    }

    fun clear() {
        notice = null
    }

    companion object {
        const val DURATION_MS = 1800L
        const val MIN_SHOW_MS = 1200L

        /** Faixas verticais (dp) do HUD superior: o aviso fica logo abaixo delas e acima da mira. */
        const val NOTICE_ABOVE_CENTER_DP = 56f
        const val NOTICE_CAP_HEIGHT_DP = 15f
        const val NEST_BAR_BOTTOM_DP = 106f
        const val FRONTS_LABEL_BOTTOM_DP = 110f

        /** Linha de base (dp) do aviso de combate para uma tela de [heightDp] dp de altura. */
        fun noticeBaselineDp(heightDp: Float): Float = heightDp / 2f - NOTICE_ABOVE_CENTER_DP
        const val PRIORITY_LOW = 0
        const val PRIORITY_NORMAL = 1
        const val PRIORITY_HIGH = 2
    }
}

/** Dica longa (parte de baixo da tela) com tempo de vida próprio; separada do aviso de combate. */
class HudTip {
    private var text: String? = null
    private var shownAtMs = 0L

    fun show(text: String, nowMs: Long) {
        this.text = text
        shownAtMs = nowMs
    }

    /** Texto e idade em ms, ou null se não há dica ou ela expirou. */
    fun current(nowMs: Long): Pair<String, Long>? {
        val t = text ?: return null
        val age = nowMs - shownAtMs
        if (age > DURATION_MS) {
            text = null
            return null
        }
        return t to age
    }

    fun clear() {
        text = null
    }

    companion object {
        const val DURATION_MS = 5000L
    }
}
