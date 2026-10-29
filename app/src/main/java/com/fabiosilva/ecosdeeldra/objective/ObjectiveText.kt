package com.fabiosilva.ecosdeeldra.objective

/** Fonte única dos textos do objetivo da campanha (HUD, telas, README usam estas frases). */
object ObjectiveText {
    const val SENTENCE = "destrua ninhos → recolha cristais → extraia pelo portal"
    const val SENTENCE_CAPITALIZED = "Destrua ninhos → recolha cristais → extraia pelo portal"
    val SENTENCE_UPPER: String = SENTENCE.uppercase()

    const val PILL_NESTS = "NINHOS"
    const val PILL_CRYSTALS = "CRISTAIS"
    const val PILL_PORTAL = "PORTAL"

    const val ACTION_DESTROY = "DESTRUA O PRÓXIMO NINHO"
    const val ACTION_PICK = "RECOLHA O CRISTAL"
    const val ACTION_EXIT = "ENTRE NO PORTAL!"

    fun nestDestroyed(done: Int, total: Int) = "NINHO $done/$total DESTRUÍDO! RECOLHA O CRISTAL"
    fun crystalPicked(done: Int, total: Int) = "CRISTAL $done/$total RECOLHIDO"
    const val PORTAL_OPEN = "PORTAL ABERTO! EXTRAIA!"
    const val LOADING_TIP = "Objetivo: destrua ninhos → recolha cristais → extraia pelo portal. Não é preciso matar todos os zumbis."
}
