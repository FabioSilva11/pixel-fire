package com.fabiosilva.ecosdeeldra.save

import com.fabiosilva.ecosdeeldra.data.Progress
import com.fabiosilva.ecosdeeldra.data.save.LoadResult
import com.fabiosilva.ecosdeeldra.data.save.Origin
import com.fabiosilva.ecosdeeldra.data.save.PersistResult
import com.fabiosilva.ecosdeeldra.data.save.SaveCodec
import com.fabiosilva.ecosdeeldra.data.save.SaveFiles
import com.fabiosilva.ecosdeeldra.data.save.SaveNotice
import com.fabiosilva.ecosdeeldra.data.save.SaveRepository
import com.fabiosilva.ecosdeeldra.save.SaveFixtures.json
import java.io.File
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder

class SaveRepositoryTest {
    @get:Rule val tmp = TemporaryFolder()
    private var now = 1_000_000L

    private fun dir() = File(tmp.root, "save").also { it.mkdirs() }
    private fun repo(legacy: () -> String? = { null }) = SaveRepository(SaveFiles(dir()), legacy, "teste", { now })
    private fun write(name: String, text: String) = File(dir(), name).writeText(text)
    private fun read(name: String) = File(dir(), name).readText()
    private fun exists(name: String) = File(dir(), name).exists()
    private fun snapshot() = dir().listFiles()!!.associate { it.name to (if (it.isFile) it.readText() else "<dir>") }
    private fun named(prefix: String) = dir().listFiles { f -> f.name.startsWith(prefix) }!!.toList()
    private fun savedValue(text: String) = (SaveCodec.decode(text) as com.fabiosilva.ecosdeeldra.data.save.DecodeOutcome.Ok).progress.settings.playerName.toIntOrNull() ?: 0

    @Test
    fun invalidJsonNeverReplacesProgressSilently() {
        write("save.json", "{corrompido")
        write("save.json.bak1", "lixo")
        write("save.json.bak2", "")
        val before = snapshot()
        val rp = repo()
        val r = rp.load()
        assertTrue(r is LoadResult.NeedsRecovery)
        assertTrue(rp.recoveryPending)
        assertEquals(PersistResult.Blocked, rp.persist(json(1)))
        assertEquals(before, snapshot())
    }

    @Test
    fun freshInstallLoadsFreshThenFirstPersistCreatesPrimary() {
        val rp = repo()
        assertEquals(LoadResult.Fresh, rp.load())
        assertEquals(PersistResult.Written, rp.persist(json(5)))
        assertEquals(5, savedValue(read("save.json")))
    }

    @Test
    fun persistThenLoadRoundTrips() {
        val a = repo()
        a.load(); a.persist(json(42))
        val r = repo().load() as LoadResult.Loaded
        assertEquals(Origin.PRIMARY, r.origin)
        assertEquals(42, (r.progress.settings.playerName.toIntOrNull() ?: 0))
        assertTrue(r.notices.isEmpty())
    }

    @Test
    fun validPrimaryWinsEvenIfBackupsAreGarbage() {
        write("save.json", json(7))
        write("save.json.bak1", "lixo"); write("save.json.bak2", "{")
        val r = repo().load() as LoadResult.Loaded
        assertEquals(Origin.PRIMARY, r.origin)
        assertEquals(7, (r.progress.settings.playerName.toIntOrNull() ?: 0))
    }

    @Test
    fun corruptPrimaryFallsBackToNewestValidBackupAndKeepsTheCorruptFile() {
        write("save.json", "{corrupt"); write("save.json.bak1", json(100)); write("save.json.bak2", json(50))
        val rp = repo()
        val r = rp.load() as LoadResult.Loaded
        assertEquals(Origin.BACKUP_1, r.origin)
        assertEquals(100, (r.progress.settings.playerName.toIntOrNull() ?: 0))
        assertTrue(r.notices.any { it is SaveNotice.RestoredFromBackup })
        assertEquals("{corrupt", read("save.json"))
        assertEquals(PersistResult.Written, rp.persist(json(101)))
        assertTrue(named("save.corrupt-").any { it.readText() == "{corrupt" })
        assertEquals(json(100), read("save.json.bak1"))
        assertEquals(101, savedValue(read("save.json")))
    }

    @Test
    fun missingPrimaryWithValidTmpUsesTmpWithoutNotice() {
        write("save.json.tmp", json(9))
        val r = repo().load() as LoadResult.Loaded
        assertEquals(Origin.INTERRUPTED_WRITE, r.origin)
        assertTrue(r.notices.isEmpty())
    }

    @Test
    fun primaryInvalidAndTmpValidStillWarns() {
        write("save.json", "{x"); write("save.json.tmp", json(9))
        val r = repo().load() as LoadResult.Loaded
        assertEquals(Origin.INTERRUPTED_WRITE, r.origin)
        assertTrue(r.notices.any { it is SaveNotice.RestoredFromBackup })
    }

    @Test
    fun allCorruptThenStartNewGameKeepsEveryBadFile() {
        write("save.json", "{a"); write("save.json.bak1", "{b"); write("save.json.bak2", "{c")
        val rp = repo()
        assertTrue(rp.load() is LoadResult.NeedsRecovery)
        val fresh = rp.startNewGame()
        assertEquals(0, (fresh.settings.playerName.toIntOrNull() ?: 0))
        assertFalse(rp.recoveryPending)
        assertEquals(0, savedValue(read("save.json")))
        assertTrue(named("save.corrupt-").any { it.readText() == "{a" })
        assertEquals("{b", read("save.json.bak1"))
        assertEquals("{c", read("save.json.bak2"))
        val again = repo().load() as LoadResult.Loaded
        assertEquals(Origin.PRIMARY, again.origin)
    }

    @Test
    fun ioErrorIsNotTreatedAsCorruption() {
        File(dir(), "save.json").mkdirs()
        val rp = repo()
        val r = rp.load() as LoadResult.NeedsRecovery
        assertTrue(r.report.readErrors)
        rp.startNewGame()
        assertTrue(named("save.unreadable-").isNotEmpty())
        assertTrue(File(dir(), "save.json").isFile)
    }

    @Test
    fun rotationRunsOnFirstSaveOfSessionThenEveryFifteenMinutes() {
        val a = json(1); val b = json(2); val c = json(3); val d = json(4)
        write("save.json", a)
        val rp = repo()
        rp.load()
        rp.persist(b)
        assertEquals(a, read("save.json.bak1"))
        rp.persist(c)
        assertEquals(a, read("save.json.bak1"))
        now += SaveRepository.BACKUP_MIN_INTERVAL_MS
        rp.persist(d)
        assertEquals(a, read("save.json.bak2"))
        assertEquals(c, read("save.json.bak1"))
    }

    @Test
    fun identicalPrimaryDoesNotRotate() {
        val a = json(1)
        write("save.json", a)
        val rp = repo()
        rp.load()
        rp.persist(a)
        assertEquals(a, read("save.json.bak1"))   // primeira gravação da sessão: ainda não havia bak1
        now += SaveRepository.BACKUP_MIN_INTERVAL_MS
        rp.persist(a)
        assertEquals(a, read("save.json.bak1"))
        assertFalse(exists("save.json.bak2"))
    }

    @Test
    fun invalidBak1IsQuarantinedAndDoesNotPushValidBak2Out() {
        write("save.json", json(1)); write("save.json.bak1", "{ruim"); write("save.json.bak2", json(2))
        val rp = repo()
        rp.load()
        rp.persist(json(3))
        assertEquals(json(2), read("save.json.bak2"))
        assertEquals(json(1), read("save.json.bak1"))
        assertTrue(named("save.corrupt-").any { it.readText() == "{ruim" })
    }

    @Test
    fun persistRefusesTextTheAppCannotReadBack() {
        write("save.json", json(1))
        val rp = repo()
        rp.load()
        assertTrue(rp.persist("{}") is PersistResult.Failed)
        assertEquals(json(1), read("save.json"))
    }

    @Test
    fun legacyPrefsAreMigratedOnceAndKeptVerbatim() {
        val rp = repo { SaveFixtures.V4_JSON }
        val r = rp.load() as LoadResult.Loaded
        assertEquals(Origin.LEGACY_PREFS, r.origin)
        assertEquals(1234, (r.progress.settings.playerName.toIntOrNull() ?: 0))
        assertTrue(exists("save.json"))
        assertEquals(SaveFixtures.V4_JSON, read("save.legacy-prefs.json"))
        val second = repo { throw IllegalStateException("não deve ler o prefs de novo") }.load() as LoadResult.Loaded
        assertEquals(Origin.PRIMARY, second.origin)
        assertEquals(1234, (second.progress.settings.playerName.toIntOrNull() ?: 0))
    }

    @Test
    fun corruptLegacyPrefsNeedRecoveryAndKeepRawCopy() {
        val rp = repo { "{quebrado" }
        assertTrue(rp.load() is LoadResult.NeedsRecovery)
        assertTrue(rp.recoveryPending)
        assertEquals("{quebrado", read("save.legacy-prefs.json"))
        assertFalse(exists("save.json"))
    }

    @Test
    fun schemaOlderThanCurrentKeepsPreMigrationSnapshotOnce() {
        write("save.json", SaveFixtures.V4_JSON)
        repo().load()
        assertEquals(SaveFixtures.V4_JSON, read("save.pre-v4.json"))
        write("save.json", SaveFixtures.v4With { it.put("inventory", "") })
        repo().load()
        assertEquals(SaveFixtures.V4_JSON, read("save.pre-v4.json"))
    }

    @Test
    fun restoreIsReversible() {
        val a = json(1); val b = json(2)
        write("save.json", a); write("save.json.bak1", b)
        val rp = repo()
        rp.load()
        val info = rp.listRestorable().first { it.file.name == "save.json.bak1" }
        val p = rp.restore(info)
        assertEquals(2, (p.settings.playerName.toIntOrNull() ?: 0))
        assertEquals(2, savedValue(read("save.json")))
        assertEquals(1, savedValue(read("save.json.bak1")))
    }


    @Test
    fun newerSchemaKeepsVerbatimCopy() {
        val raw = SaveFixtures.v4With { it.put("version", 99) }
        write("save.json", raw)
        val r = repo().load() as LoadResult.Loaded
        assertTrue(r.notices.any { it is SaveNotice.NewerSchema })
        assertEquals(raw, read("save.newer-v99.json"))
        assertNotNull(r.progress)
    }

    @Test
    fun progressIsNeverUnintendedFreshWhenFileIsValid() {
        write("save.json", SaveCodec.encode(Progress().apply { settings.playerName = "321" }))
        assertEquals(321, (repo().load() as LoadResult.Loaded).progress.settings.playerName.toIntOrNull() ?: 0)
    }
}
