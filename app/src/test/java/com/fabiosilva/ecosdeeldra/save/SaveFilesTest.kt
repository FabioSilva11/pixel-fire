package com.fabiosilva.ecosdeeldra.save

import com.fabiosilva.ecosdeeldra.data.save.FileRead
import com.fabiosilva.ecosdeeldra.data.save.SaveFiles
import java.io.File
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder

class SaveFilesTest {
    @get:Rule val tmp = TemporaryFolder()
    private fun files() = SaveFiles(File(tmp.root, "save"))

    @Test
    fun writeTmpThenPromoteReplacesPrimaryAndLeavesNoTmp() {
        val f = files()
        f.writeTmp("um"); f.promoteTmp()
        f.writeTmp("dois"); f.promoteTmp()
        assertEquals("dois", f.primary.readText())
        assertFalse(f.tmp.exists())
    }

    @Test
    fun copyAtomicKeepsModifiedTimeAndLeavesNoPart() {
        val f = files()
        f.dir.mkdirs()
        f.primary.writeText("abc")
        f.primary.setLastModified(1_700_000_000_000L)
        f.copyAtomic(f.primary, f.bak1)
        assertEquals("abc", f.bak1.readText())
        assertEquals(1_700_000_000_000L, f.bak1.lastModified())
        assertFalse(File(f.bak1.path + ".part").exists())
    }

    @Test
    fun quarantineIsDedupedByContentAndCappedAtFive() {
        val f = files()
        val a = f.quarantine("x", "corrupt")!!
        val b = f.quarantine("x", "corrupt")!!
        assertEquals(a, b)
        for (i in 0 until 10) f.quarantine("texto $i", "corrupt")
        val n = f.dir.listFiles { x -> x.name.startsWith("save.corrupt-") }!!.size
        assertTrue("havia $n", n <= SaveFiles.QUARANTINE_KEEP)
    }

    @Test
    fun readReportsMissingTooBigAndIoErrorSeparately() {
        val f = files()
        f.dir.mkdirs()
        assertEquals(FileRead.Missing, f.read(f.primary))
        f.primary.writeBytes(ByteArray((SaveFiles.MAX_BYTES + 1).toInt()))
        assertTrue(f.read(f.primary) is FileRead.TooBig)
        f.primary.delete()
        f.primary.mkdirs()
        assertTrue(f.read(f.primary) is FileRead.Failed)
    }
}
