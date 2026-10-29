package com.fabiosilva.ecosdeeldra.data.save

import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

object SaveMessages {
    private fun date(ms: Long): String = SimpleDateFormat("dd/MM 'às' HH:mm", Locale.forLanguageTag("pt-BR")).format(Date(ms))

    /** (título, mensagem) ou null se o aviso não deve aparecer ao jogador. */
    fun describe(n: SaveNotice): Pair<String, String>? = when (n) {
        is SaveNotice.RestoredFromBackup -> "Save recuperado" to
            "O arquivo principal do seu save estava danificado ou ausente. Restauramos a cópia de segurança de ${date(n.savedAt)} " +
            "(${n.summary.equipmentSlots} espaços de equipamento ocupados). O arquivo danificado foi guardado, sem alterações, na pasta do jogo."
        is SaveNotice.SectionsReset -> "Parte do save foi reiniciada" to
            "Estas partes estavam ilegíveis e voltaram ao início: ${n.sections.joinToString(", ")}. O resto do progresso foi mantido e o original ficou guardado."
        is SaveNotice.NewerSchema -> "Save de uma versão mais nova" to
            "Este save foi criado por uma versão mais nova do jogo (formato ${n.found}). Ele foi aberto e o original foi guardado; atualize o jogo para não perder dados novos."
    }
}
