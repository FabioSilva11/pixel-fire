package com.fabiosilva.ecosdeeldra.save

import com.fabiosilva.ecosdeeldra.data.save.Origin
import com.fabiosilva.ecosdeeldra.data.save.SaveMessages
import com.fabiosilva.ecosdeeldra.data.save.SaveNotice
import com.fabiosilva.ecosdeeldra.data.save.SaveSummary
import org.junit.Assert.assertTrue
import org.junit.Test

class SaveMessagesTest {
    @Test
    fun everyNoticeHasTitleAndText() {
        val notices = listOf(
            SaveNotice.RestoredFromBackup(Origin.BACKUP_1, 1_760_000_000_000L, SaveSummary(3)),
            SaveNotice.SectionsReset(listOf("equipamento")),
            SaveNotice.NewerSchema(99),
        )
        for (n in notices) {
            val (t, m) = SaveMessages.describe(n)!!
            assertTrue(t.isNotBlank() && m.isNotBlank())
        }
    }
}
