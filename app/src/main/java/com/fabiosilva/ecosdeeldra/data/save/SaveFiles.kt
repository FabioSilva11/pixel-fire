package com.fabiosilva.ecosdeeldra.data.save

import java.io.File
import java.io.FileOutputStream
import java.io.IOException
import java.util.zip.CRC32

sealed interface FileRead {
    data object Missing : FileRead
    data class Text(val text: String) : FileRead
    data class TooBig(val bytes: Long) : FileRead
    data class Failed(val error: Throwable) : FileRead
}

class SaveFiles(val dir: File) {
    val primary = File(dir, "save.json")
    val tmp = File(dir, "save.json.tmp")
    val bak1 = File(dir, "save.json.bak1")
    val bak2 = File(dir, "save.json.bak2")

    /** Nunca lança: ausência, tamanho absurdo e erro de E/S são resultados distintos. */
    fun read(f: File): FileRead = try {
        when {
            !f.exists() -> FileRead.Missing
            f.isFile && f.length() > MAX_BYTES -> FileRead.TooBig(f.length())
            else -> FileRead.Text(f.readBytes().toString(Charsets.UTF_8))   // diretório no lugar do arquivo cai em Failed
        }
    } catch (e: Exception) {
        FileRead.Failed(e)
    }

    fun writeTmp(text: String) {
        dir.mkdirs()
        FileOutputStream(tmp).use { out ->
            out.write(text.toByteArray(Charsets.UTF_8))
            out.flush()
            out.fd.sync()
        }
    }

    fun promoteTmp() = moveOver(tmp, primary)

    /** rename(2) substitui o destino de forma atômica no Android/Linux; o desvio para Windows serve só aos testes JVM. */
    fun moveOver(from: File, to: File) {
        if (from.renameTo(to)) return
        if (IS_WINDOWS && to.exists() && to.delete() && from.renameTo(to)) return
        throw IOException("não foi possível mover ${from.name} para ${to.name}")
    }

    /** Cópia que nunca deixa um destino pela metade: grava em .part, sincroniza e só então renomeia. */
    fun copyAtomic(src: File, dst: File) {
        val part = File(dst.path + ".part")
        src.inputStream().use { input ->
            FileOutputStream(part).use { out ->
                input.copyTo(out)
                out.flush()
                out.fd.sync()
            }
        }
        part.setLastModified(src.lastModified())
        moveOver(part, dst)
    }

    /** Guarda uma cópia do texto ruim (dedup por CRC, no máximo 5 por prefixo). Melhor esforço: null se falhar. */
    fun quarantine(text: String, prefix: String): File? = try {
        dir.mkdirs()
        val f = File(dir, "save.$prefix-${crc(text)}.json")
        if (!f.exists()) {
            f.writeText(text, Charsets.UTF_8)
            prune("save.$prefix-", QUARANTINE_KEEP)
        }
        f
    } catch (e: Exception) {
        null
    }

    /** Grava o arquivo só se ainda não existir (snapshots de migração). Melhor esforço. */
    fun keepOnce(name: String, text: String): File? = try {
        dir.mkdirs()
        val f = File(dir, name)
        if (!f.exists()) f.writeText(text, Charsets.UTF_8)
        f
    } catch (e: Exception) {
        null
    }

    /** Arquivo ilegível (E/S): em vez de apagar, só renomeia. Lança se não conseguir. */
    fun setAside(f: File, tag: String, nowMs: Long): File {
        val dst = File(dir, "save.$tag-$nowMs-${f.name}")
        if (!f.renameTo(dst)) throw IOException("não foi possível separar ${f.name}")
        return dst
    }

    fun copyAllTo(target: File): Int {
        target.mkdirs()
        var n = 0
        dir.listFiles()?.filter { it.isFile }?.forEach { it.copyTo(File(target, it.name), overwrite = true); n++ }
        return n
    }

    private fun prune(prefix: String, keep: Int) {
        val all = dir.listFiles { f -> f.name.startsWith(prefix) } ?: return
        all.sortedByDescending { it.lastModified() }.drop(keep).forEach { it.delete() }
    }

    private fun crc(text: String): String {
        val c = CRC32()
        c.update(text.toByteArray(Charsets.UTF_8))
        return c.value.toString(16).padStart(8, '0')
    }

    companion object {
        const val MAX_BYTES = 4L * 1024 * 1024
        const val QUARANTINE_KEEP = 5
        private val IS_WINDOWS = System.getProperty("os.name").orEmpty().startsWith("Windows")
    }
}
