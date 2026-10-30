package com.fabiosilva.ecosdeeldra.data

class Settings {
    var sensitivity = 1.0f
    var musicVolume = 0.6f
    var sfxVolume = 1.0f
    var aimAssist = true
    var invertY = false
    var vibration = true
    var quality = 1
    var showFps = false
    var leftHanded = false
    var playerName = ""
    var hudLayout = ""
    var hudOpacity = 0.85f
    var previewWeapon = 3
}

/** Estado persistente do equipamento e controles. */
class Progress {
    val lookCode: Int get() = Look().encode()
    val inventory = Inventory.starter()
    val settings = Settings()
}
