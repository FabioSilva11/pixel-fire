package com.fabiosilva.ecosdeeldra.ui

import android.annotation.SuppressLint
import android.graphics.Color
import android.graphics.drawable.GradientDrawable
import android.view.Gravity
import android.view.View
import android.widget.FrameLayout
import android.widget.LinearLayout
import com.fabiosilva.ecosdeeldra.MainActivity

object Header {
    fun build(app: MainActivity, title: String, subtitle: String, onBack: () -> Unit): LinearLayout {
        val row = Ui.row(app)
        row.setPadding(app.dpi(16), app.dpi(10), app.dpi(16), 0)
        val titles = Ui.column(app)
        titles.addView(Ui.text(app, subtitle, 9f, Palette.accent, Fonts.title))
        titles.addView(Ui.title(app, title, 24f))
        row.addView(titles, Ui.lp(0, Ui.WRAP, 1f))
        row.addView(Ui.iconButton(app, Icon.CLOSE, Palette.stone, Color.WHITE, onBack), Ui.lp(app.dpi(44), app.dpi(44)))
        return row
    }
}

/** O cenário e o personagem continuam desenhados pelo motor 3D. */
@SuppressLint("ViewConstructor")
class HomeScreen(private val app: MainActivity) : FrameLayout(app) {
    private val queueStatus=Ui.text(app,"Conectando…",12f,Palette.text)
    private var formation=1
    private var fill=true
    fun status(text: String){queueStatus.text=text}

    init {
        val shade = View(app).apply {
            background = GradientDrawable(GradientDrawable.Orientation.TOP_BOTTOM,
                intArrayOf(Color.argb(170, 8, 4, 24), Color.TRANSPARENT))
        }
        addView(shade, Ui.frame(Ui.MATCH, app.dpi(90), Gravity.TOP))
        val brand = Ui.title(app, "ZOMBIE ZONE", 20f, Palette.accent)
        addView(brand, Ui.frame(Ui.WRAP, Ui.WRAP, Gravity.TOP or Gravity.START).apply {
            setMargins(app.dpi(20), app.dpi(14), 0, 0)
        })
        val settings = Ui.iconButton(app, Icon.GEAR, Palette.stone, Color.WHITE) { app.showSettings() }
        addView(settings, Ui.frame(app.dpi(42), app.dpi(42), Gravity.TOP or Gravity.END).apply {
            setMargins(0, app.dpi(12), app.dpi(20), 0)
        })
        val weapons = Ui.button(app, "ARMAS", Palette.stone, Color.WHITE, 18f) { app.showInventory() }
        addView(weapons, Ui.frame(app.dpi(150), app.dpi(44), Gravity.BOTTOM or Gravity.START).apply {
            setMargins(app.dpi(20), 0, 0, app.dpi(18))
        })
        val matchmaking=Ui.column(app)
        val modes=Ui.row(app)
        listOf(1 to "SOLO",2 to "DUO",4 to "SQUAD").forEach { (size,label)->
            modes.addView(Ui.button(app,label,Palette.stone,Color.WHITE,13f){formation=size;app.selectFormation(size,fill);queueStatus.text="${label} selecionado"},Ui.lp(0,app.dpi(38),1f))
        }
        matchmaking.addView(modes)
        val autofill=android.widget.CheckBox(app).apply{text="Completar equipe automaticamente";textSize=12f;setTextColor(Color.WHITE);isChecked=true;setOnCheckedChangeListener{_,checked->fill=checked;app.selectFormation(formation,fill)}}
        matchmaking.addView(autofill)
        val friends=Ui.row(app)
        friends.addView(Ui.button(app,"CRIAR GRUPO",Palette.stone,Color.WHITE,11f){app.group("create")},Ui.lp(0,app.dpi(38),1f))
        friends.addView(Ui.button(app,"ENTRAR POR CÓDIGO",Palette.stone,Color.WHITE,11f){
            val input=android.widget.EditText(app).apply{hint="Código do grupo";inputType=android.text.InputType.TYPE_CLASS_TEXT;filters=arrayOf(android.text.InputFilter.LengthFilter(8))}
            android.app.AlertDialog.Builder(app).setTitle("Entrar no grupo").setView(input).setPositiveButton("ENTRAR"){_,_->app.group("join",input.text.toString())}.setNegativeButton("CANCELAR",null).show()
        },Ui.lp(0,app.dpi(38),1f))
        friends.addView(Ui.button(app,"SAIR DO GRUPO",Palette.stone,Color.WHITE,11f){app.group("leave")},Ui.lp(0,app.dpi(38),1f))
        matchmaking.addView(friends)
        matchmaking.addView(queueStatus)
        matchmaking.addView(Ui.button(app,"CANCELAR BUSCA",Palette.stone,Color.WHITE,11f){app.cancelSearch()},Ui.lp(app.dpi(160),app.dpi(32)))
        val panelWidth=(resources.displayMetrics.widthPixels/resources.displayMetrics.density*.36f).toInt().coerceIn(280,300)
        addView(matchmaking,Ui.frame(app.dpi(panelWidth),Ui.WRAP,Gravity.BOTTOM or Gravity.END).apply{bottomMargin=app.dpi(76);rightMargin=app.dpi(20)})
        val play = Ui.button(app, "JOGAR", Palette.accent, Palette.dark, 24f) { app.startGame() }
        addView(play, Ui.frame(app.dpi(170), app.dpi(48), Gravity.BOTTOM or Gravity.END).apply {
            setMargins(0, 0, app.dpi(20), app.dpi(16))
        })
    }
}
