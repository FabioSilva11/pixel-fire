package com.fabiosilva.ecosdeeldra.ui

import android.annotation.SuppressLint
import android.graphics.Color
import android.view.Gravity
import android.widget.FrameLayout
import android.widget.ScrollView
import com.fabiosilva.ecosdeeldra.MainActivity
import com.fabiosilva.ecosdeeldra.engine.NativeBridge

@SuppressLint("ViewConstructor")
class ArmoryScreen(private val app: MainActivity,private val back: ()->Unit): FrameLayout(app){
 init {
  val column=Ui.column(app).apply{setBackgroundColor(Color.argb(200,7,14,25))}
  column.addView(Header.build(app,"ARMAS","PRÉVIA NO LOBBY",back))
  column.addView(Ui.text(app,"Na partida, encontre suas armas e munições pelo mapa.",12f,Palette.muted))
  val list=Ui.column(app)
  val descriptions=listOf("12 tiros • munição de pistola","30 tiros • munição de SMG","30 tiros • munição de fuzil","8 tiros • cartuchos","5 tiros • munição de precisão","Corpo a corpo")
  for(id in 1..6){
   val button=Ui.button(app,"${BattleItems.name(id)}\n${descriptions[id-1]}",if(app.progress.settings.previewWeapon==id)Palette.accent else Palette.stone,Color.WHITE,14f){
    app.progress.settings.previewWeapon=id;app.save();app.engine{NativeBridge.previewWeapon(id)}
    app.showInventory(back)
   }
   list.addView(button,Ui.lp(Ui.MATCH,app.dpi(54)).apply{bottomMargin=app.dpi(6)})
  }
  column.addView(ScrollView(app).apply{addView(list)},Ui.lp(Ui.MATCH,0,1f))
  addView(column,Ui.frame(app.dpi(365),Ui.MATCH,Gravity.START))
 }
}
