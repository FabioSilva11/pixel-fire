package com.fabiosilva.ecosdeeldra.audio

import android.content.Context
import android.media.AudioAttributes
import android.media.MediaPlayer
import android.media.SoundPool
import android.os.SystemClock
import com.fabiosilva.ecosdeeldra.R
import com.fabiosilva.ecosdeeldra.engine.Snd

/**
 * Efeitos com SoundPool (baixa latência, OGG em res/raw) e música com MediaPlayer em loop.
 * play() é seguro para chamar da thread GL.
 */
class GameAudio(private val context: Context) {
    enum class Music { NONE, MENU, BATTLE }

    private val attributes = AudioAttributes.Builder()
        .setUsage(AudioAttributes.USAGE_GAME)
        .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
        .build()
    private val pool = SoundPool.Builder().setMaxStreams(16).setAudioAttributes(attributes).build()
    private val engineSounds = IntArray(Snd.COUNT)
    private val groanAlt: Int
    private val lastPlayed = LongArray(Snd.COUNT)
    private val ui = mutableMapOf<String, Int>()

    private var player: MediaPlayer? = null
    private var currentMusic = Music.NONE
    private var wantedMusic = Music.NONE
    private var paused = false

    var sfxVolume = 1f
    var musicVolume = 0.6f
        set(value) {
            field = value
            player?.setVolume(value, value)
        }

    init {
        fun load(id: Int) = pool.load(context, id, 1)
        engineSounds[Snd.SHOT] = load(R.raw.sfx_shot)
        engineSounds[Snd.RELOAD] = load(R.raw.sfx_reload)
        engineSounds[Snd.HIT] = load(R.raw.sfx_hit)
        engineSounds[Snd.HEADSHOT] = load(R.raw.sfx_headshot)
        engineSounds[Snd.GROAN] = load(R.raw.sfx_groan1)
        engineSounds[Snd.ZOMBIE_DIE] = load(R.raw.sfx_zombie_die)
        engineSounds[Snd.HURT] = load(R.raw.sfx_hurt)
        engineSounds[Snd.PICKUP] = load(R.raw.sfx_pickup)
        engineSounds[Snd.PORTAL] = load(R.raw.sfx_portal)
        engineSounds[Snd.JUMP] = load(R.raw.sfx_jump)
        engineSounds[Snd.STEP] = load(R.raw.sfx_step)
        engineSounds[Snd.EMPTY] = load(R.raw.sfx_empty)
        engineSounds[Snd.SPLASH] = load(R.raw.sfx_splash)
        engineSounds[Snd.ZOMBIE_ATTACK] = load(R.raw.sfx_zombie_attack)
        engineSounds[Snd.HEAL] = load(R.raw.sfx_heal)
        engineSounds[Snd.THROW] = load(R.raw.sfx_throw)
        engineSounds[Snd.FIRE] = load(R.raw.sfx_fire)
        engineSounds[Snd.EXPLOSION] = load(R.raw.sfx_explosion)
        engineSounds[Snd.KNIFE] = load(R.raw.sfx_knife)
        engineSounds[Snd.SCREAM] = load(R.raw.sfx_scream)
        engineSounds[Snd.ROCK] = load(R.raw.sfx_rock)
        engineSounds[Snd.CHEST] = load(R.raw.sfx_chest)
        engineSounds[Snd.SPIT] = load(R.raw.sfx_spit)
        engineSounds[Snd.RACK] = load(R.raw.sfx_rack)
        engineSounds[Snd.SHOTGUN] = load(R.raw.sfx_shotgun)
        engineSounds[Snd.EQUIP] = load(R.raw.sfx_equip)
        engineSounds[Snd.WAVE] = load(R.raw.sfx_wave)
        groanAlt = load(R.raw.sfx_groan2)
        ui["click"] = load(R.raw.ui_click)
        ui["reward"] = load(R.raw.sfx_reward)
        ui["victory"] = load(R.raw.sfx_victory)
        ui["defeat"] = load(R.raw.sfx_defeat)
        ui["unlock"] = load(R.raw.sfx_unlock)
    }

    /** Som vindo do motor: volume 0..100 e pan -100..100. */
    fun playEngine(sound: Int, volume: Int, pan: Int) {
        if (sound !in engineSounds.indices || sfxVolume <= 0f) return
        val now = SystemClock.uptimeMillis()
        // Evita empilhar o mesmo som no mesmo instante (ex.: rajada + vários impactos).
        val minGap = when (sound) { Snd.SHOT -> 45L; Snd.WAVE -> 1500L; Snd.STEP -> 120L; Snd.GROAN -> 350L; Snd.EXPLOSION -> 90L; Snd.FIRE -> 120L; Snd.RELOAD, Snd.RACK -> 250L; Snd.EMPTY -> 200L; else -> 30L }
        if (now - lastPlayed[sound] < minGap) return
        lastPlayed[sound] = now
        val v = (volume / 100f).coerceIn(0f, 1f) * sfxVolume
        val p = (pan / 100f).coerceIn(-1f, 1f)
        val left = v * (1f - p.coerceAtLeast(0f))
        val right = v * (1f + p.coerceAtMost(0f))
        var id = engineSounds[sound]
        if (sound == Snd.GROAN && (now and 1L) == 0L) id = groanAlt
        val rate = when (sound) {
            Snd.GROAN, Snd.ZOMBIE_DIE -> 0.85f + (now % 7) * 0.05f
            Snd.HIT, Snd.STEP, Snd.ROCK -> 0.9f + (now % 5) * 0.05f
            Snd.SCREAM, Snd.THROW -> 0.92f + (now % 4) * 0.05f
            // Os SoundDef de tiro do template variam um pouco o tom a cada disparo.
            Snd.SHOT, Snd.SHOTGUN -> 0.95f + (now % 5) * 0.025f
            else -> 1f
        }
        val priority = if (sound == Snd.SHOT || sound == Snd.HURT || sound == Snd.EXPLOSION || sound == Snd.WAVE || sound == Snd.SHOTGUN) 2 else 1
        pool.play(id, left, right, priority, 0, rate)
    }

    fun playUi(name: String, volume: Float = 0.8f) {
        val id = ui[name] ?: return
        pool.play(id, volume * sfxVolume, volume * sfxVolume, 3, 0, 1f)
    }

    fun setMusic(music: Music) {
        wantedMusic = music
        if (paused) return
        if (music == currentMusic && player != null) return
        player?.release()
        player = null
        currentMusic = music
        val res = when (music) {
            Music.MENU -> R.raw.music_menu
            Music.BATTLE -> R.raw.music_battle
            Music.NONE -> return
        }
        player = MediaPlayer.create(context, res)?.apply {
            isLooping = true
            setVolume(musicVolume, musicVolume)
            start()
        }
    }

    fun pause() {
        paused = true
        player?.takeIf { it.isPlaying }?.pause()
        pool.autoPause()
    }

    fun resume() {
        paused = false
        pool.autoResume()
        if (player == null || currentMusic != wantedMusic) setMusic(wantedMusic) else player?.start()
    }

    fun release() {
        player?.release()
        player = null
        pool.release()
    }
}
