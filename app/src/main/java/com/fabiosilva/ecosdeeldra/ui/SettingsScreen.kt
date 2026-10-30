package com.fabiosilva.ecosdeeldra.ui

import android.annotation.SuppressLint
import android.content.res.ColorStateList
import android.graphics.Color
import android.view.Gravity
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.SeekBar
import android.widget.Switch
import com.fabiosilva.ecosdeeldra.MainActivity

@SuppressLint("ViewConstructor", "UseSwitchCompatOrMaterialCode")
class SettingsScreen(private val app: MainActivity, private val fromPause: Boolean) : FrameLayout(app) {
    private val s = app.progress.settings

    init {
        setBackgroundColor(Color.argb(if (fromPause) 220 else 170, 6, 10, 20))
        Ui.blockTouches(this)
        val column = Ui.column(app)
        column.addView(Header.build(app, "AJUSTES", "CONTROLES • SOM • GRÁFICOS") {
            app.applySettings()
            if (fromPause) app.pauseMission() else app.showHome()
        })
        val list = Ui.column(app)
        val left = Ui.column(app)
        val right = Ui.column(app)

        left.addView(section("CONTROLES"))
        left.addView(slider("Sensibilidade da câmera", (s.sensitivity * 50).toInt(), 125) { s.sensitivity = (it / 50f).coerceAtLeast(0.3f) })
        left.addView(toggle("Inverter eixo vertical", "", s.invertY) { s.invertY = it })
        left.addView(Ui.button(app, "✥ PERSONALIZAR CONTROLES (HUD)", Palette.cyan, Palette.dark, 13f) { app.applySettings(); app.showHudEditor(fromPause) }, Ui.lp(Ui.MATCH, app.dpi(46)).apply { bottomMargin = app.dpi(4) })
        left.addView(Ui.text(app, "Mova, aumente ou esconda cada botão: atirar, mirar, pular, agachar, correr, faca, granada...", 10f, Palette.muted).apply { setPadding(0, 0, 0, app.dpi(8)) })
        left.addView(toggle("Modo canhoto", "Espelha o layout padrão (apaga o layout personalizado)", s.leftHanded) { s.leftHanded = it; s.hudLayout = "" })
        left.addView(toggle("Vibração", "Ao levar dano e acertar tiros", s.vibration) { s.vibration = it })
        right.addView(section("SOM"))
        right.addView(slider("Música", (s.musicVolume * 100).toInt(), 100) { s.musicVolume = it / 100f; app.audio.musicVolume = s.musicVolume })
        right.addView(slider("Efeitos", (s.sfxVolume * 100).toInt(), 100) { s.sfxVolume = it / 100f; app.audio.sfxVolume = s.sfxVolume })
        right.addView(section("GRÁFICOS"))
        val qualityRow = Ui.row(app)
        listOf("BAIXA", "MÉDIA", "ALTA").forEachIndexed { i, label ->
            val b = Ui.button(app, label, if (s.quality == i) Palette.lime else Palette.stone, if (s.quality == i) Palette.dark else Color.WHITE, 12f) {
                s.quality = i
                app.applySettings()
                if (fromPause) app.showPauseSettings() else app.showSettings()
            }
            qualityRow.addView(b, Ui.lp(0, app.dpi(42), 1f).apply { marginEnd = if (i < 2) app.dpi(6) else 0 })
        }
        right.addView(qualityRow, Ui.lp(Ui.MATCH, Ui.WRAP).apply { bottomMargin = app.dpi(4) })
        right.addView(Ui.text(app, "Qualidade muda a resolução e o detalhe das construções distantes.", 10f, Palette.muted).apply { setPadding(0, 0, 0, app.dpi(8)) })
        right.addView(toggle("Mostrar FPS", "", s.showFps) { s.showFps = it })
        right.addView(Ui.text(app, "Personagens, mundo, texturas, sons e músicas gerados para o Zombie Zone.", 10f, Palette.muted).apply { setPadding(0, app.dpi(10), 0, 0) })

        val columns = Ui.row(app, Gravity.TOP)
        columns.addView(left, Ui.lp(0, Ui.WRAP, 1f).apply { marginEnd = app.dpi(16) })
        columns.addView(right, Ui.lp(0, Ui.WRAP, 1f))
        list.addView(columns)
        val scroll = ScrollView(app).apply { addView(list) }
        column.addView(scroll, Ui.lp(Ui.MATCH, 0, 1f).apply { setMargins(app.dpi(20), app.dpi(10), app.dpi(20), app.dpi(10)) })
        addView(column, Ui.frame(Ui.MATCH, Ui.MATCH))
    }

    private fun section(title: String) = Ui.text(app, title, 11f, Palette.lime, Fonts.title).apply { setPadding(0, app.dpi(8), 0, app.dpi(6)) }

    private fun slider(label: String, value: Int, max: Int, onChange: (Int) -> Unit): LinearLayout {
        val box = Ui.panel(app, Palette.panelSoft)
        val title = Ui.text(app, label, 13f, Palette.text, Fonts.bold)
        box.addView(title)
        box.addView(SeekBar(app).apply {
            this.max = max
            progress = value
            progressTintList = ColorStateList.valueOf(Palette.lime)
            thumbTintList = ColorStateList.valueOf(Palette.lime)
            setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                override fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) { if (fromUser) onChange(progress) }
                override fun onStartTrackingTouch(seekBar: SeekBar?) = Unit
                override fun onStopTrackingTouch(seekBar: SeekBar?) { app.applySettings() }
            })
        })
        return box.apply { layoutParams = Ui.lp(Ui.MATCH, Ui.WRAP).apply { bottomMargin = app.dpi(8) } }
    }

    private fun toggle(label: String, hint: String, value: Boolean, onChange: (Boolean) -> Unit): LinearLayout {
        val box = Ui.panel(app, Palette.panelSoft)
        val row = Ui.row(app)
        val texts = Ui.column(app)
        texts.addView(Ui.text(app, label, 13f, Palette.text, Fonts.bold))
        if (hint.isNotEmpty()) texts.addView(Ui.text(app, hint, 10f, Palette.muted))
        row.addView(texts, Ui.lp(0, Ui.WRAP, 1f))
        row.addView(Switch(app).apply {
            isChecked = value
            thumbTintList = ColorStateList.valueOf(Palette.lime)
            trackTintList = ColorStateList.valueOf(Palette.stone)
            setOnCheckedChangeListener { _, checked ->
                onChange(checked)
                app.applySettings()
            }
        })
        box.addView(row)
        return box.apply { layoutParams = Ui.lp(Ui.MATCH, Ui.WRAP).apply { bottomMargin = app.dpi(8) } }
    }
}
