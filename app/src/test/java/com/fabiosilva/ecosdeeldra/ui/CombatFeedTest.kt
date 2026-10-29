package com.fabiosilva.ecosdeeldra.ui

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class CombatFeedTest {
    @Test
    fun latestNoticeReplacesPreviousAndExpires() {
        val feed = CombatFeed()
        assertNull(feed.current(0))
        feed.show("+1 ABATE", 1, 1_000)
        feed.show("TIRO NA CABEÇA!", 2, 1_400)
        val now = feed.current(1_500)
        assertEquals("TIRO NA CABEÇA!", now?.text)
        assertEquals(2, now?.color)
        // a mensagem antiga não volta e a nova vale 1800 ms a partir de quando apareceu
        assertTrue(feed.current(1_400 + CombatFeed.DURATION_MS - 1) != null)
        assertNull(feed.current(1_400 + CombatFeed.DURATION_MS))
        assertNull(feed.current(1_500))
    }

    @Test
    fun lowPriorityDoesNotEraseFreshImportantNotice() {
        val feed = CombatFeed()
        feed.show("DESTRUA OS 3 NINHOS!", 1, 1_000, CombatFeed.PRIORITY_HIGH)
        feed.show("+1 ABATE", 2, 1_300, CombatFeed.PRIORITY_LOW)
        assertEquals("DESTRUA OS 3 NINHOS!", feed.current(1_400)?.text)
        // depois de MIN_SHOW_MS o aviso fraco pode substituir
        feed.show("+1 ABATE", 2, 1_000 + CombatFeed.MIN_SHOW_MS, CombatFeed.PRIORITY_LOW)
        assertEquals("+1 ABATE", feed.current(2_300)?.text)
        // prioridade igual ou maior sempre substitui
        feed.show("PORTAL ABERTO!", 3, 2_400, CombatFeed.PRIORITY_HIGH)
        assertEquals("PORTAL ABERTO!", feed.current(2_500)?.text)
    }

    @Test
    fun noticeBandIsBelowNestBarAndFrontsOnShortScreens() {
        for (h in listOf(360f, 392f, 411f)) {
            val capTop = CombatFeed.noticeBaselineDp(h) - CombatFeed.NOTICE_CAP_HEIGHT_DP
            assertTrue("h=$h", capTop >= CombatFeed.NEST_BAR_BOTTOM_DP)
            assertTrue("h=$h", capTop >= CombatFeed.FRONTS_LABEL_BOTTOM_DP - 1f)
            // acima do braço superior da mira (cy - 48 dp)
            assertTrue("h=$h", CombatFeed.noticeBaselineDp(h) < h / 2f - 48f)
        }
    }

    @Test
    fun tipExpiresAfterFiveSeconds() {
        val tip = HudTip()
        tip.show("dica", 100)
        assertEquals("dica", tip.current(4_000)?.first)
        assertNull(tip.current(100 + HudTip.DURATION_MS + 1))
    }
}
