package com.fabiosilva.ecosdeeldra.data.save

import com.fabiosilva.ecosdeeldra.data.Progress
import java.io.File

/** De onde veio o progresso carregado. */
enum class Origin { PRIMARY, INTERRUPTED_WRITE, BACKUP_1, BACKUP_2, LEGACY_PREFS }

data class SaveSummary(val equipmentSlots: Int)

/** Um arquivo válido que o usuário pode restaurar (backup, snapshot de migração, save antigo). */
data class BackupInfo(val label: String, val file: File, val modifiedAt: Long, val summary: SaveSummary)

enum class CandidateStatus { INVALID, UNREADABLE }

data class RecoveryCandidate(
    val label: String,
    val fileName: String,
    val status: CandidateStatus,
    val detail: String,
    val sizeBytes: Long,
    val modifiedAt: Long,
)

data class RecoveryReport(
    val candidates: List<RecoveryCandidate>,
    /** Algum arquivo não pôde ser LIDO (E/S): "tentar de novo" é a ação principal. */
    val readErrors: Boolean,
    /** Itens válidos que o usuário pode restaurar mesmo com o primário/backups ilegíveis. */
    val restorable: List<BackupInfo>,
)

/** Avisos não bloqueantes mostrados depois da tela inicial. */
sealed interface SaveNotice {
    data class RestoredFromBackup(val origin: Origin, val savedAt: Long, val summary: SaveSummary) : SaveNotice
    data class SectionsReset(val sections: List<String>) : SaveNotice
    data class NewerSchema(val found: Int) : SaveNotice
}

sealed interface LoadResult {
    /** Instalação nova: nada no disco nem no formato antigo. Único caso em que "jogo novo" é automático. */
    data object Fresh : LoadResult
    data class Loaded(val progress: Progress, val origin: Origin, val notices: List<SaveNotice>) : LoadResult
    /** Há arquivos, mas nenhum utilizável. O disco não foi tocado; gravações ficam bloqueadas. */
    data class NeedsRecovery(val report: RecoveryReport) : LoadResult
}

sealed interface PersistResult {
    data object Written : PersistResult
    /** Recuperação pendente: nada foi escrito (de propósito). */
    data object Blocked : PersistResult
    data class Failed(val error: Throwable) : PersistResult
}
