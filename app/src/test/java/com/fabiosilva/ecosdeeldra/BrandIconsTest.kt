package com.fabiosilva.ecosdeeldra

import com.fabiosilva.ecosdeeldra.ui.BrandIcons
import java.io.File
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class BrandIconsTest {
    @Test
    fun mapa_tem_todos_os_icones_e_recursos_distintos() {
        assertEquals(23, BrandIcons.icons.size)
        assertEquals(BrandIcons.icons.size, BrandIcons.icons.values.toSet().size)
        assertEquals(3, BrandIcons.art.size)
        assertNotNull(BrandIcons.resId(BrandIcons.NINHO))
        assertNotNull(BrandIcons.resId(BrandIcons.LOGO))
        assertNull(BrandIcons.resId("nao-existe"))
    }

    @Test
    fun nomes_sao_minusculos_sem_acento() {
        for (n in BrandIcons.icons.keys + BrandIcons.art.keys) assertTrue(n, Regex("[a-z]+(-[a-z]+)*").matches(n))
    }

    @Test
    fun familias_tem_icones_distintos() {
        // familyColor usa Palette (android.graphics.Color) e só roda no aparelho.
        val icones = (0..2).map { BrandIcons.familyIcon(it) }.toSet()
        assertEquals(3, icones.size)
        assertEquals(BrandIcons.familyIcon(0), BrandIcons.familyIcon(99))
        for (i in icones) assertNotNull(BrandIcons.resId(i))
    }

    @Test
    fun svg_mestre_existe_para_cada_nome_quando_visivel() {
        // Roda com cwd = app/; se o repositório não estiver visível (CI isolado), não falha.
        val dir = File("../docs/brand/svg")
        if (!dir.isDirectory) return
        for (n in BrandIcons.icons.keys + BrandIcons.art.keys) assertTrue(n, File(dir, "$n.svg").isFile)
    }
}
