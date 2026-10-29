package com.fabiosilva.ecosdeeldra.ui

import android.annotation.SuppressLint
import android.graphics.Color
import android.view.Gravity
import android.widget.FrameLayout
import android.widget.ScrollView
import com.fabiosilva.ecosdeeldra.MainActivity
import com.fabiosilva.ecosdeeldra.data.save.BackupInfo
import com.fabiosilva.ecosdeeldra.data.save.CandidateStatus
import com.fabiosilva.ecosdeeldra.data.save.LoadResult
import com.fabiosilva.ecosdeeldra.data.save.RecoveryReport
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/** Mostrada no arranque quando nenhum arquivo de save é utilizável. Nada é gravado até o usuário escolher. */
@SuppressLint("ViewConstructor")
class RecoveryScreen(private val app: MainActivity, private var report: RecoveryReport) : FrameLayout(app) {
    init {
        setBackgroundColor(Palette.bg)
        Ui.blockTouches(this)
        render()
    }

    private fun render() {
        removeAllViews()
        val left = Ui.column(app)
        left.addView(Ui.text(app, "PROTEÇÃO DO PROGRESSO", 10f, Palette.accent, Fonts.title))
        left.addView(Ui.title(app, "NÃO FOI POSSÍVEL ABRIR O SAVE", 22f, Palette.red))
        left.addView(Ui.text(app, if (report.readErrors) READ_TEXT else CORRUPT_TEXT, 13f).apply {
            setLineSpacing(0f, 1.2f)
            setPadding(0, app.dpi(8), 0, app.dpi(10))
        })
        report.candidates.forEach { c ->
            val state = if (c.status == CandidateStatus.UNREADABLE) "não foi possível ler" else "ilegível"
            left.addView(Ui.text(app, "• ${c.label} (${c.fileName}): $state — ${c.detail}", 10f, Palette.muted).apply { setPadding(0, 0, 0, app.dpi(3)) })
        }

        val right = Ui.column(app)
        right.addView(action("TENTAR DE NOVO", Palette.lime, Palette.dark) { retry() })
        report.restorable.forEach { b ->
            right.addView(action("RESTAURAR: ${b.label.uppercase()}", Palette.cyan, Palette.dark) { restore(b) })
            right.addView(Ui.text(app, "${b.summary.equipmentSlots} espaços de equipamento • ${date(b.modifiedAt)}", 10f, Palette.muted).apply { setPadding(0, 0, 0, app.dpi(8)) })
        }
        right.addView(action("COMEÇAR JOGO NOVO", Palette.gold, Palette.dark) { confirmNewGame() })
        right.addView(action("COPIAR ARQUIVOS PARA SUPORTE", Palette.stone, Color.WHITE) { export() })
        right.addView(action("FECHAR O JOGO", Palette.stone, Color.WHITE) { app.finish() })

        val columns = Ui.row(app, Gravity.TOP)
        columns.addView(left, Ui.lp(0, Ui.WRAP, 1f).apply { marginEnd = app.dpi(16) })
        columns.addView(right, Ui.lp(0, Ui.WRAP, 1f))
        columns.setPadding(app.dpi(24), app.dpi(16), app.dpi(24), app.dpi(16))
        addView(ScrollView(app).apply { addView(columns) }, Ui.frame(Ui.MATCH, Ui.MATCH))
    }

    private fun action(label: String, fill: Int, textColor: Int, onClick: () -> Unit) =
        Ui.button(app, label, fill, textColor, 13f, onClick).also { it.layoutParams = Ui.lp(Ui.MATCH, app.dpi(46)).apply { bottomMargin = app.dpi(8) } }

    private fun retry() = guarded {
        when (val r = app.store.load()) {
            is LoadResult.Loaded -> app.onRecoveryResolved(r.progress, r.notices)
            is LoadResult.Fresh -> app.onRecoveryResolved(app.store.startNewGame())
            is LoadResult.NeedsRecovery -> { report = r.report; render() }
        }
    }

    private fun restore(b: BackupInfo) = guarded { app.onRecoveryResolved(app.store.restore(b)) }

    private fun confirmNewGame() {
        Popup.show(app, "Começar um jogo novo?", "Os arquivos do save atual continuam guardados na pasta do jogo (nada é apagado), mas você começará do zero.", "COMEÇAR NOVO", "VOLTAR") { ok ->
            if (ok) guarded { app.onRecoveryResolved(app.store.startNewGame()) }
        }
    }

    private fun export() {
        val dir = app.store.exportForSupport()
        Popup.show(app, if (dir != null) "Arquivos copiados" else "Não foi possível copiar", dir?.let { "Copiados para:\n${it.absolutePath}" } ?: "Armazenamento externo indisponível.", "OK")
    }

    private fun guarded(block: () -> Unit) {
        try { block() } catch (e: Exception) {
            Popup.show(app, "Não foi possível concluir", e.message ?: "Erro ao gravar o save. Libere espaço e tente de novo.", "OK")
        }
    }

    private fun date(ms: Long) = SimpleDateFormat("dd/MM HH:mm", Locale.forLanguageTag("pt-BR")).format(Date(ms))

    private companion object {
        const val CORRUPT_TEXT = "Não conseguimos ler o save principal nem as cópias de segurança. Seu progresso NÃO foi apagado: os arquivos foram mantidos sem alteração. Escolha abaixo."
        const val READ_TEXT = "O armazenamento não respondeu ao ler o save. Feche outros apps, libere espaço e toque em TENTAR DE NOVO. Nada foi apagado."
    }
}
