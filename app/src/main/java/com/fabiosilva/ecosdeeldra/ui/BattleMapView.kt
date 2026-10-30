package com.fabiosilva.ecosdeeldra.ui

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.*
import android.view.MotionEvent
import android.view.View
import kotlin.math.min

@SuppressLint("ViewConstructor","ClickableViewAccessibility")
class BattleMapView(context: Context,private val image: Bitmap?,private val state: FloatArray,private val localId: Int,private val mark: (Float,Float)->Unit): View(context){
 private val p=Paint(Paint.ANTI_ALIAS_FLAG).apply{typeface=Fonts.title;textAlign=Paint.Align.CENTER}
 private val regionNames=arrayOf("Vila Rubra","Templo da Areia","Mirante Escarlate","Pedras Antigas","Pátio Central","Fortaleza Seca","Beco de Poeira","Usina de Ferro","Cais do Sol","Base Eclipse","Vale dos Ossos","Casco Perdido","Rocha do Tesouro")
 private val coords=arrayOf(220 to 220,690 to 190,1160 to 220,430 to 410,720 to 720,1190 to 650,220 to 700,960 to 440,180 to 1190,1160 to 1160,470 to 1020,740 to 1240,950 to 940)
 private val rect=RectF()
 private fun point(x: Float,z: Float)=PointF(rect.left+x/1440*rect.width(),rect.top+z/1440*rect.height())
 override fun onDraw(c: Canvas){
  c.drawColor(Color.argb(245,5,12,20));val d=resources.displayMetrics.density;val size=min(width.toFloat()-24*d,height.toFloat()-48*d)
  rect.set((width-size)/2,(height-size)/2,(width+size)/2,(height+size)/2)
  p.color=Color.rgb(35,55,40);c.drawRect(rect,p);image?.let{c.drawBitmap(it,null,rect,null)}
  val own=(0 until state[16].toInt()).map{24+it*32}.firstOrNull{state[it].toInt()==localId}
  val zoneRevealed=state[11]>0f||state[12]>0f||state[1].toInt()==2||(state[1].toInt()==1&&(own==null||state[own+11].toInt()==3)&&state[22]>=30f)
  c.save();c.clipRect(rect)
  if(zoneRevealed){val waitProg=if(state[1].toInt()==2)(state[22]/(state[22]+state[10]).coerceAtLeast(30f)).coerceIn(0f,1f) else 0f;val initWait=state[11].toInt()==0&&state[12]<=0f;val zx=if(initWait)720f else state[4];val zz=if(initWait)720f else state[5];val zr=if(initWait)(1050f+(state[6]-1050f)*waitProg) else state[6];p.style=Paint.Style.STROKE;p.strokeWidth=2.2f*d;p.color=Palette.cyan;point(zx,zz).let{c.drawCircle(it.x,it.y,zr/1440*size,p)};p.strokeWidth=1.8f*d;p.color=Color.WHITE;point(state[7],state[8]).let{c.drawCircle(it.x,it.y,state[9]/1440*size,p)}}
  p.style=Paint.Style.FILL
  p.textSize=9*d;p.color=Color.WHITE
  coords.forEachIndexed{i,coord->point(coord.first.toFloat(),coord.second.toFloat()).let{c.drawText(regionNames[i],it.x,it.y,p)}}
  if(own!=null)repeat(state[16].toInt()){val at=24+it*32;if(state[at+1]==state[own+1]){p.color=if(at==own)Color.WHITE else Palette.lime;point(state[at+4],state[at+6]).let{c.drawCircle(it.x,it.y,4*d,p)};if(state[at+24]>=0){p.color=Palette.gold;point(state[at+24],state[at+25]).let{c.drawCircle(it.x,it.y,4*d,p)}}}}
  var at=24+state[16].toInt()*32+state[17].toInt()*7+state[18].toInt()*10
  repeat(state[19].toInt()){p.color=Palette.red;point(state[at+1],state[at+3]).let{c.drawRect(it.x-4*d,it.y-4*d,it.x+4*d,it.y+4*d,p)};at+=7}
  if(state[1]==1f){p.color=Palette.gold;point(state[13],state[15]).let{c.drawCircle(it.x,it.y,5*d,p)}}
  c.restore();p.color=Color.WHITE;p.textSize=12*d;c.drawText("TERRA ESCARLATE • toque no mapa para marcar • toque fora para fechar",width/2f,height-8*d,p)
 }
 override fun onTouchEvent(e: MotionEvent): Boolean{if(e.actionMasked==MotionEvent.ACTION_UP){if(!rect.contains(e.x,e.y)){mark(-1f,-1f);return true};val x=((e.x-rect.left)/rect.width()*1440).coerceIn(0f,1439f);val z=((e.y-rect.top)/rect.height()*1440).coerceIn(0f,1439f);mark(x,z)};return true}
}
