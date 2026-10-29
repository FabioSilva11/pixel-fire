package com.fabiosilva.ecosdeeldra.ui

import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.PorterDuff
import android.graphics.drawable.Drawable
import com.fabiosilva.ecosdeeldra.R

/**
 * Família de ícones da identidade (docs/brand/IDENTIDADE.md). Os drawables `zz_*` vêm de
 * docs/brand/svg via tools/svg2vd.py; os ícones 24x24 são brancos e se tingem por código.
 * Nomes = nomes dos SVGs mestres (pt-BR, sem acento).
 */
object BrandIcons {
    // contratos
    const val RECUPERACAO_SILENCIOSA = "recuperacao-silenciosa"
    const val NINHO_INSTAVEL = "ninho-instavel"
    const val NOITE_VERMELHA = "noite-vermelha"
    const val CAIXA_PERDIDA = "caixa-perdida"
    const val RESISTENCIA = "resistencia"
    // objetivos
    const val NINHO = "ninho"
    const val CRISTAL = "cristal"
    const val PORTAL_EXTRACAO = "portal-extracao"
    // famílias de ninho
    const val FAMILIA_HORDA = "familia-horda"
    const val FAMILIA_EXPLOSIVO = "familia-explosivo"
    const val FAMILIA_TOXICO = "familia-toxico"
    // recursos
    const val SUCATA = "sucata"
    const val MATERIAL_INFECTADO = "material-infectado"
    // implantáveis
    const val BARRICADA_DOBRAVEL = "barricada-dobravel"
    const val ISCA_SONORA = "isca-sonora"
    const val KIT_DE_CAMPO = "kit-de-campo"
    // utilitários
    const val COMPARTILHAR = "compartilhar"
    const val CODIGO_QR = "codigo-qr"
    const val RELATORIO = "relatorio"
    const val BUSSOLA = "bussola"
    const val CAVEIRA = "caveira"
    const val RELOGIO = "relogio"
    const val COMBO = "combo"
    // marca (coloridos, NÃO tintar)
    const val WORDMARK = "wordmark"
    const val LOGO = "logo"
    const val SELO = "selo"

    /** nome -> recurso. Só ícones 24x24 tintáveis. */
    val icons: Map<String, Int> = linkedMapOf(
        RECUPERACAO_SILENCIOSA to R.drawable.zz_ic_silent_recovery,
        NINHO_INSTAVEL to R.drawable.zz_ic_unstable_nest,
        NOITE_VERMELHA to R.drawable.zz_ic_red_night,
        CAIXA_PERDIDA to R.drawable.zz_ic_lost_crate,
        RESISTENCIA to R.drawable.zz_ic_endurance,
        NINHO to R.drawable.zz_ic_nest,
        CRISTAL to R.drawable.zz_ic_crystal,
        PORTAL_EXTRACAO to R.drawable.zz_ic_extraction_portal,
        FAMILIA_HORDA to R.drawable.zz_ic_family_horde,
        FAMILIA_EXPLOSIVO to R.drawable.zz_ic_family_explosive,
        FAMILIA_TOXICO to R.drawable.zz_ic_family_toxic,
        SUCATA to R.drawable.zz_ic_scrap,
        MATERIAL_INFECTADO to R.drawable.zz_ic_infected_material,
        BARRICADA_DOBRAVEL to R.drawable.zz_ic_barricade,
        ISCA_SONORA to R.drawable.zz_ic_sound_lure,
        KIT_DE_CAMPO to R.drawable.zz_ic_field_kit,
        COMPARTILHAR to R.drawable.zz_ic_share,
        CODIGO_QR to R.drawable.zz_ic_qr,
        RELATORIO to R.drawable.zz_ic_report,
        BUSSOLA to R.drawable.zz_ic_compass,
        CAVEIRA to R.drawable.zz_ic_skull,
        RELOGIO to R.drawable.zz_ic_clock,
        COMBO to R.drawable.zz_ic_combo,
    )

    /** Arte de marca em cores próprias (sem tinta). */
    val art: Map<String, Int> = linkedMapOf(
        WORDMARK to R.drawable.zz_wordmark,
        LOGO to R.drawable.zz_logo,
        SELO to R.drawable.zz_seal,
    )

    /** Cor da família de ninho (0 horda, 1 explosivo, 2 tóxico); índice inválido cai na horda. */
    fun familyColor(family: Int): Int = when (family) {
        1 -> Palette.famExplosive
        2 -> Palette.famToxic
        else -> Palette.famHorde
    }

    fun familyIcon(family: Int): String = when (family) {
        1 -> FAMILIA_EXPLOSIVO
        2 -> FAMILIA_TOXICO
        else -> FAMILIA_HORDA
    }

    fun resId(name: String): Int? = icons[name] ?: art[name]

    private val cache = HashMap<Int, Drawable.ConstantState>()

    private fun obtain(ctx: Context, res: Int): Drawable? {
        val state = cache[res] ?: ctx.applicationContext.getDrawable(res)?.constantState?.also { cache[res] = it }
        return state?.newDrawable(ctx.resources)?.mutate()
    }

    /** Desenha o ícone [name] num quadrado [size] px com canto em ([x], [y]). [tint] = 0 mantém as cores da arte. */
    fun draw(canvas: Canvas, ctx: Context, name: String, x: Float, y: Float, size: Float, tint: Int = 0xFFFFFFFF.toInt(), alpha: Int = 255) {
        val res = resId(name) ?: return
        val d = obtain(ctx, res) ?: return
        d.setBounds(x.toInt(), y.toInt(), (x + size).toInt(), (y + size).toInt())
        if (name in icons) d.setColorFilter(tint, PorterDuff.Mode.SRC_IN)
        d.alpha = alpha
        d.draw(canvas)
    }

    /** Bitmap [px] x [px] (cartão de resultado, notificações). */
    fun toBitmap(ctx: Context, name: String, px: Int, tint: Int = 0xFFFFFFFF.toInt()): Bitmap {
        val bmp = Bitmap.createBitmap(px, px, Bitmap.Config.ARGB_8888)
        draw(Canvas(bmp), ctx, name, 0f, 0f, px.toFloat(), tint)
        return bmp
    }
}
