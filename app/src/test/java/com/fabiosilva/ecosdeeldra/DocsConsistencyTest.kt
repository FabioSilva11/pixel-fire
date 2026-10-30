package com.fabiosilva.ecosdeeldra

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Assume.assumeTrue
import org.junit.Test
import java.io.File

/**
 * Impede o README e os textos do jogo de voltarem a divergir do motor: objetivo oficial, versão do app,
 * e regras antigas.
 */
class DocsConsistencyTest {
    private fun find(vararg candidates: String): File? = candidates.map(::File).firstOrNull { it.exists() }

    private fun readme(): String? = find("../README.md", "README.md")?.readText()
    private fun gradle(): String? = find("build.gradle.kts", "app/build.gradle.kts")?.readText()
    private fun sourceRoot(): File? = find("src/main/java", "app/src/main/java")

    @Test
    fun readmeStatesTheOfficialObjective() {
        val text = readme()
        assumeTrue("README.md não encontrado", text != null)
        assertTrue("README sem a frase oficial", text!!.contains("Battle Royale online"))
    }

    @Test
    fun readmeVersionMatchesGradle() {
        val text = readme()
        val build = gradle()
        assumeTrue("README.md ou build.gradle.kts não encontrado", text != null && build != null)
        val name = Regex("""versionName\s*=\s*"([^"]+)"""").find(build!!)?.groupValues?.get(1)
        val code = Regex("""versionCode\s*=\s*(\d+)""").find(build)?.groupValues?.get(1)
        assertTrue("versionName/versionCode não achados no gradle", name != null && code != null)
        assertTrue("README deve citar: Versão `$name` (versionCode $code)", text!!.contains("Versão `$name` (versionCode $code)"))
    }

    @Test
    fun readmeHasNoObsoleteRules() {
        val text = readme()
        assumeTrue("README.md não encontrado", text != null)
        val obsolete = listOf(
            "eliminar todos os zumbis", "Eles caem dos zumbis", "20 fases", "Kenney", "acampamento mais distante",
            "três tipos", "assets/characters", "são **de teste**", "Dificuldade adaptativa",
        )
        for (s in obsolete) assertFalse("README ainda contém: $s", text!!.contains(s))
    }



    @Test
    fun sourcesDoNotRepeatObsoleteObjectiveTexts() {
        val root = sourceRoot()
        assumeTrue("src/main/java não encontrado", root != null)
        val forbidden = listOf("DESTRUA OS 3 NINHOS", "setas roxas", "PEGUE O CRISTAL", "CRISTAL COLETADO", "Colete 3 cristais")
        val restam = Regex("""RESTAM\s+\$\{?alive""")
        val offenders = ArrayList<String>()
        root!!.walkTopDown().filter { it.isFile && it.extension == "kt" }.forEach { f ->
            val text = f.readText()
            for (s in forbidden) if (text.contains(s)) offenders.add("${f.name}: $s")
            if (restam.containsMatchIn(text)) offenders.add("${f.name}: RESTAM \$alive")
        }
        assertEquals("textos obsoletos em: $offenders", 0, offenders.size)
    }
}
