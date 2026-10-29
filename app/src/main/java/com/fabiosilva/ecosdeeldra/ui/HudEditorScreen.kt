package com.fabiosilva.ecosdeeldra.ui

import android.annotation.SuppressLint
import android.graphics.Color
import android.view.Gravity
import android.widget.FrameLayout
import android.widget.TextView
import com.fabiosilva.ecosdeeldra.MainActivity

/**
 * Editor do HUD: o jogador arrasta cada controle para onde quiser, muda o tamanho,
 * esconde os opcionais e ajusta a transparência. Só vale depois de SALVAR.
 */
@SuppressLint("ViewConstructor")
class HudEditorScreen(private val app: MainActivity, private val fromPause: Boolean) : FrameLayout(app) {
    private val settings = app.progress.settings
    private val hud = HudView(app, object : HudView.Listener {
        override fun onPauseRequested() = Unit
        override fun onMapRequested() = Unit
    })
    private val selectedLabel = Ui.text(app, "Toque num controle", 12f, Palette.gold, Fonts.title)
    private val opacityLabel = Ui.text(app, "", 11f, Palette.text, Fonts.bold)
    private val hideButton: TextView
    private var opacity = settings.hudOpacity

    init {
        setBackgroundColor(Color.argb(150, 4, 7, 14))
        isClickable = true
        hud.applySettings(settings.leftHanded, false, false, settings.hudLayout, opacity)
        hud.editing = true
        addView(hud, Ui.frame(Ui.MATCH, Ui.MATCH))

        val bar = Ui.panel(app, Color.parseColor("#EE0E1628"))
        val row = Ui.row(app)
        row.addView(selectedLabel.apply { setPadding(0, 0, app.dpi(8), 0) }, Ui.lp(app.dpi(150), Ui.WRAP))
        fun small(label: String, fill: Int = Palette.stone, text: Int = Color.WHITE, onClick: () -> Unit) =
            Ui.button(app, label, fill, text, 13f, onClick).apply { setPadding(app.dpi(8), app.dpi(4), app.dpi(8), app.dpi(8)) }
        row.addView(small("−") { hud.scaleSelected(-0.1f) }, Ui.lp(app.dpi(42), app.dpi(38)))
        row.addView(Ui.text(app, "TAMANHO", 9f, Palette.muted, Fonts.title).apply { gravity = Gravity.CENTER }, Ui.lp(app.dpi(58), Ui.WRAP))
        row.addView(small("+") { hud.scaleSelected(0.1f) }, Ui.lp(app.dpi(42), app.dpi(38)))
        hideButton = small("OCULTAR") {
            val visible = hud.toggleSelectedVisible()
            if (visible == null) app.audio.playUi("click", 0.3f) else refreshHide()
        }
        row.addView(hideButton, Ui.lp(app.dpi(96), app.dpi(38)).apply { marginStart = app.dpi(8) })
        row.addView(small("PADRÃO") { hud.setWorkingLayout(hud.defaultLayout()); refreshHide() }, Ui.lp(app.dpi(84), app.dpi(38)).apply { marginStart = app.dpi(8) })
        row.addView(small("CANCELAR") { close() }, Ui.lp(app.dpi(96), app.dpi(38)).apply { marginStart = app.dpi(8) })
        row.addView(small("SALVAR ✓", Palette.lime, Palette.dark) { save() }, Ui.lp(app.dpi(96), app.dpi(38)).apply { marginStart = app.dpi(8) })
        bar.addView(row)
        val second = Ui.row(app)
        second.addView(Ui.text(app, "Arraste os controles para posicionar • toque num controle para mudar tamanho ou ocultar", 10f, Palette.muted), Ui.lp(0, Ui.WRAP, 1f))
        second.addView(small("−") { setOpacity(opacity - 0.1f) }, Ui.lp(app.dpi(36), app.dpi(32)))
        second.addView(opacityLabel.apply { gravity = Gravity.CENTER }, Ui.lp(app.dpi(120), Ui.WRAP))
        second.addView(small("+") { setOpacity(opacity + 0.1f) }, Ui.lp(app.dpi(36), app.dpi(32)))
        bar.addView(second, Ui.lp(Ui.MATCH, Ui.WRAP).apply { topMargin = app.dpi(6) })
        addView(bar, Ui.frame(Ui.WRAP, Ui.WRAP, Gravity.TOP or Gravity.CENTER_HORIZONTAL).apply { topMargin = app.dpi(6) })

        hud.onSelectionChanged = { refreshHide() }
        setOpacity(opacity)
        refreshHide()
    }

    private fun setOpacity(value: Float) {
        opacity = (Math.round(value * 10f) / 10f).coerceIn(0.3f, 1f)
        hud.opacity = opacity
        hud.invalidate()
        opacityLabel.text = "TRANSPARÊNCIA ${(opacity * 100).toInt()}%"
    }

    private fun refreshHide() {
        val c = hud.selected
        selectedLabel.text = c?.label?.uppercase() ?: "Toque num controle"
        val optional = c?.optional == true
        hideButton.text = if (optional && !hud.layout[c!!].visible) "MOSTRAR" else "OCULTAR"
        Ui.setEnabledLook(hideButton, optional)
    }

    private fun save() {
        settings.hudLayout = hud.layout.encode()
        settings.hudOpacity = opacity
        app.applySettings()
        app.audio.playUi("reward", 0.7f)
        close()
    }

    private fun close() {
        if (fromPause) app.showPauseSettings() else app.closeOverlay()
    }
}
