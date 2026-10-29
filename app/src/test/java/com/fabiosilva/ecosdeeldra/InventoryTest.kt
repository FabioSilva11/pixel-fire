package com.fabiosilva.ecosdeeldra

import com.fabiosilva.ecosdeeldra.data.Inventory
import com.fabiosilva.ecosdeeldra.data.Items
import com.fabiosilva.ecosdeeldra.engine.INV_INT_COUNT
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import kotlin.random.Random

class InventoryTest {
    @Test
    fun starterKitPutsWeaponsAndItemsOnTheHotbar() {
        val inv = Inventory.starter()
        assertEquals(Items.RIFLE, inv[0].id)
        assertEquals(Items.SHOTGUN, inv[1].id)
        assertEquals(3, inv.count(Items.GRENADE))
        assertEquals(0, inv.weaponSlot)
        assertTrue((0 until Inventory.HOTBAR).all { !inv[it].empty || it == 5 })
    }

    @Test
    fun itemsStackUpToTheLimitAndOverflowIsReturned() {
        val inv = Inventory()
        assertEquals(0, inv.add(Items.MEDKIT, 7))
        assertEquals(3, inv[0].count)
        assertEquals(3, inv[1].count)
        assertEquals(1, inv[2].count)
        // Granadas crescem com a melhoria de explosivos (+1 a cada 3 níveis).
        assertEquals(8, Items.maxStack(Items.GRENADE, 0))
        assertEquals(10, Items.maxStack(Items.GRENADE, 6))
        // Mochila cheia devolve o excesso.
        repeat(Inventory.SIZE) { inv.add(Items.BANDAGE, 10) }
        assertTrue(inv.add(Items.ANTIDOTE, 1) == 1)
        assertFalse(inv.canAdd(Items.ANTIDOTE))
    }

    @Test
    fun weaponsNeverDuplicateAndCannotBeDiscarded() {
        val inv = Inventory.starter()
        assertEquals(0, inv.add(Items.RIFLE, 1))
        assertEquals(1, inv.count(Items.RIFLE))
        assertFalse(inv.discard(0))
        assertTrue(inv.discard(3)) // bandagens
        assertTrue(inv[3].empty)
    }

    @Test
    fun moveSwapsSlotsAndKeepsTheWeaponInHand() {
        val inv = Inventory.starter()
        inv.weaponSlot = 1 // escopeta
        inv.moveOrSwap(1, 10)
        assertEquals(Items.SHOTGUN, inv[10].id)
        assertEquals(10, inv.weaponSlot)
        // Juntar pilhas iguais.
        inv.add(Items.BANDAGE, 1)
        val first = inv.slots.indexOfFirst { it.id == Items.BANDAGE }
        inv.slots[15].id = Items.BANDAGE
        inv.slots[15].count = 2
        inv.moveOrSwap(15, first)
        assertEquals(5, inv[first].count)
        assertTrue(inv[15].empty)
    }

    @Test
    fun encodeDecodeAndEngineArrayRoundTrip() {
        val inv = Inventory.starter()
        inv.add(Items.ADRENALINE, 1)
        inv.throwSlot = 2
        val copy = Inventory.decode(inv.encode())
        assertEquals(inv.encode(), copy.encode())
        val arr = inv.toIntArray()
        assertEquals(INV_INT_COUNT, arr.size)
        assertEquals(Items.RIFLE, arr[0])
        assertEquals(2, arr[INV_INT_COUNT - 1])
        // Inventário sem armas recebe o fuzil.
        val empty = Inventory.decode("0:0|0|-1")
        assertEquals(1, empty.count(Items.RIFLE))
        // Armas antigas excluídas (ids 2 e 4..8) somem ao carregar um save velho.
        val legacy = Inventory.decode("5:1,3:1,8:1|0|-1")
        assertEquals(0, legacy.count(5) + legacy.count(8))
        assertEquals(Items.SHOTGUN, legacy[legacy.weaponSlot].id)
    }

    @Test
    fun savedPistolBecomesTheShotgunInTheSameSlot() {
        // Save antigo: pistola (id 2) no espaço 1, sem escopeta.
        val inv = Inventory.decode("1:1,2:1,21:2|0|-1")
        assertEquals(Items.SHOTGUN, inv[1].id)
        assertEquals(1, inv[1].count)
        assertEquals(Items.RIFLE, inv[0].id)
        assertEquals(2, inv.count(Items.BANDAGE))
        assertTrue(inv.slots.none { it.id == 2 })
    }

    @Test
    fun savedInventoryWithoutShotgunGetsOneOnTheHotbar() {
        val inv = Inventory.decode("1:1,21:2|0|-1")
        assertEquals(1, inv.count(Items.SHOTGUN))
        assertTrue(inv.firstSlotOf(Items.SHOTGUN) in 0 until Inventory.HOTBAR)
    }

    @Test
    fun legacyPistolDoesNotDuplicateASavedShotgun() {
        val inv = Inventory.decode("1:1,2:1,3:1|0|-1")
        assertEquals(1, inv.count(Items.SHOTGUN))
        assertTrue(inv.slots.none { it.id == 2 })
    }

}
