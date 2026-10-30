package com.fabiosilva.ecosdeeldra.ui

import android.graphics.Bitmap
import androidx.compose.animation.core.*
import androidx.compose.foundation.*
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.CutCornerShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.*
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.viewinterop.AndroidView
import com.fabiosilva.ecosdeeldra.MainActivity
import com.fabiosilva.ecosdeeldra.data.Look
import com.fabiosilva.ecosdeeldra.data.LookPainter
import com.fabiosilva.ecosdeeldra.data.save.BackupInfo
import com.fabiosilva.ecosdeeldra.data.save.CandidateStatus
import com.fabiosilva.ecosdeeldra.data.save.LoadResult
import com.fabiosilva.ecosdeeldra.data.save.RecoveryReport
import com.fabiosilva.ecosdeeldra.engine.NativeBridge
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import kotlin.math.cos
import kotlin.math.min

// ----------------------------------------------------------------------------- Paleta Compose
private val ColGold = Color(0xFFFFC61A)
private val ColGoldBright = Color(0xFFFFDA38)
private val ColGoldDeep = Color(0xFFE09600)
private val ColPanelDark = Color(0xD9121724)
private val ColPanelGlass = Color(0xCC151B2B)
private val ColBorder = Color(0xFF3E4B66)
private val ColBorderLight = Color(0xFF5A6B8C)
private val ColCyan = Color(0xFF28C4FF)
private val ColLime = Color(0xFF9EFF3B)
private val ColRed = Color(0xFFFF3B30)
private val ColMuted = Color(0xFF94A3B8)

private enum class LobbyDialog { NONE, LOJA, EQUIPE, PERSONAGEM }

@Composable
fun LobbyComposeScreen(
    app: MainActivity,
    statusText: String,
    teamSize: Int,
    autoFill: Boolean,
    searching: Boolean,
    previewWeapon: Int,
    onSelectFormation: (Int, Boolean) -> Unit,
    onStartOrCancel: () -> Unit,
) {
    var activeDialog by remember { mutableStateOf(LobbyDialog.NONE) }
    val look = remember(app.progress.lookCode) { Look.decode(app.progress.lookCode) }
    val faceBitmap = remember(look) { LookPainter.face(look).asImageBitmap() }
    val playerName = remember(app.progress.settings.playerName) {
        app.progress.settings.playerName.ifBlank { "ETZ_KIRITO" }
    }

    Box(modifier = Modifier.fillMaxSize()) {
        // Sombra suave no topo e na base para destacar os menus em primeiro plano
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .height(72.dp)
                .align(Alignment.TopCenter)
                .background(
                    Brush.verticalGradient(
                        listOf(Color(0x99060912), Color.Transparent)
                    )
                )
        )
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .height(84.dp)
                .align(Alignment.BottomCenter)
                .background(
                    Brush.verticalGradient(
                        listOf(Color.Transparent, Color(0xAA060912))
                    )
                )
        )

        // 1. TOPO ESQUERDO: Perfil (Avatar + Nome + Nv. 67 + Barra XP) + Moedas + Diamantes
        TopProfileAndCurrencyBar(
            playerName = playerName,
            faceBitmap = faceBitmap,
            onProfileClick = {
                app.audio.playUi("click", 0.6f)
                activeDialog = LobbyDialog.PERSONAGEM
            },
            onPlusClick = {
                app.audio.playUi("click", 0.6f)
                activeDialog = LobbyDialog.LOJA
            },
            modifier = Modifier
                .align(Alignment.TopStart)
                .padding(start = 12.dp, top = 8.dp)
        )

        // 2. TOPO DIREITO: Botão de Configurações (Engrenagem)
        Box(
            modifier = Modifier
                .align(Alignment.TopEnd)
                .padding(end = 14.dp, top = 8.dp)
                .size(width = 48.dp, height = 38.dp)
                .clip(RoundedCornerShape(6.dp))
                .background(ColPanelDark)
                .border(1.dp, ColBorder, RoundedCornerShape(6.dp))
                .clickable {
                    app.audio.playUi("click", 0.6f)
                    app.showSettings()
                },
            contentAlignment = Alignment.Center
        ) {
            Canvas(modifier = Modifier.size(20.dp)) {
                drawGearIcon(Color.White)
            }
        }

        // 3. COLUNA ESQUERDA: Menus com ícones (LOJA, PASSE, MISSÕES, EVENTOS, EQUIPE)
        Column(
            modifier = Modifier
                .align(Alignment.CenterStart)
                .padding(start = 14.dp, top = 12.dp),
            verticalArrangement = Arrangement.spacedBy(7.dp)
        ) {
            LobbyLeftMenuButton(
                label = "LOJA",
                highlighted = true,
                badge = false,
                icon = { drawCartIcon(ColGold) },
                onClick = {
                    app.audio.playUi("click", 0.6f)
                    app.showInventory()
                }
            )
            LobbyLeftMenuButton(
                label = "PASSE",
                highlighted = false,
                badge = false,
                icon = { drawPassBadgeIcon(Color.White) },
                onClick = { app.audio.playUi("click", 0.6f) }
            )
            LobbyLeftMenuButton(
                label = "MISSÕES",
                highlighted = false,
                badge = false,
                icon = { drawMissionsIcon(Color.White) },
                onClick = { app.audio.playUi("click", 0.6f) }
            )
            LobbyLeftMenuButton(
                label = "EVENTOS",
                highlighted = false,
                badge = false,
                icon = { drawCalendarIcon(Color.White) },
                onClick = { app.audio.playUi("click", 0.6f) }
            )
            LobbyLeftMenuButton(
                label = "EQUIPE",
                highlighted = false,
                badge = false,
                icon = { drawTeamIcon(Color.White, 3) },
                onClick = {
                    app.audio.playUi("click", 0.6f)
                    activeDialog = LobbyDialog.EQUIPE
                }
            )
        }

        // 4. BASE ESQUERDA: Barra COFRE | PERSONAGEM
        BottomLeftVaultCharacterBar(
            onVaultClick = {
                app.audio.playUi("click", 0.6f)
                app.showInventory()
            },
            onCharacterClick = {
                app.audio.playUi("click", 0.6f)
                activeDialog = LobbyDialog.PERSONAGEM
            },
            modifier = Modifier
                .align(Alignment.BottomStart)
                .padding(start = 14.dp, bottom = 10.dp)
        )

        // 5. DIREITA / BASE DIREITA: Card BR RANQUEADO (Solo / Duo / Squad) + Botão JOGAR
        RightMatchmakingPanel(
            statusText = statusText,
            teamSize = teamSize,
            searching = searching,
            onSelectMode = { size ->
                app.audio.playUi("click", 0.6f)
                onSelectFormation(size, autoFill)
            },
            onOpenTeamModal = {
                app.audio.playUi("click", 0.6f)
                activeDialog = LobbyDialog.EQUIPE
            },
            onPlayClick = {
                app.audio.playUi("click", 0.7f)
                onStartOrCancel()
            },
            modifier = Modifier
                .align(Alignment.BottomEnd)
                .padding(end = 14.dp, bottom = 10.dp)
        )

        // Modais dos menus do lobby
        when (activeDialog) {
            LobbyDialog.EQUIPE -> PartyComposeModal(
                app = app,
                statusText = statusText,
                teamSize = teamSize,
                autoFill = autoFill,
                onSelectFormation = onSelectFormation,
                onDismiss = { activeDialog = LobbyDialog.NONE }
            )
            LobbyDialog.PERSONAGEM -> CharacterWeaponComposeModal(
                app = app,
                selectedWeapon = previewWeapon,
                onDismiss = { activeDialog = LobbyDialog.NONE }
            )
            LobbyDialog.LOJA -> CharacterWeaponComposeModal(
                app = app,
                selectedWeapon = previewWeapon,
                onDismiss = { activeDialog = LobbyDialog.NONE }
            )
            LobbyDialog.NONE -> Unit
        }
    }
}

// ----------------------------------------------------------------------------- Componentes do Lobby

@Composable
private fun TopProfileAndCurrencyBar(
    playerName: String,
    faceBitmap: ImageBitmap,
    onProfileClick: () -> Unit,
    onPlusClick: () -> Unit,
    modifier: Modifier = Modifier,
) {
    Row(
        modifier = modifier,
        verticalAlignment = Alignment.CenterVertically
    ) {
        // Card do Jogador (Avatar + Nome + Nv. 67 + Barra XP)
        Row(
            modifier = Modifier
                .height(46.dp)
                .clip(CutCornerShape(topStart = 4.dp, bottomStart = 4.dp, topEnd = 6.dp, bottomEnd = 10.dp))
                .background(
                    Brush.horizontalGradient(
                        listOf(Color(0xE61B2336), Color(0xD9141A29))
                    )
                )
                .border(
                    1.dp,
                    ColBorderLight,
                    CutCornerShape(topStart = 4.dp, bottomStart = 4.dp, topEnd = 6.dp, bottomEnd = 10.dp)
                )
                .clickable(onClick = onProfileClick)
                .padding(horizontal = 6.dp, vertical = 4.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Box(
                modifier = Modifier
                    .size(36.dp)
                    .clip(RoundedCornerShape(4.dp))
                    .background(Color(0xFF3A2618))
                    .border(1.5.dp, Color(0xFFD6933A), RoundedCornerShape(4.dp)),
                contentAlignment = Alignment.Center
            ) {
                Image(
                    bitmap = faceBitmap,
                    contentDescription = null,
                    filterQuality = FilterQuality.None,
                    contentScale = ContentScale.Crop,
                    modifier = Modifier.fillMaxSize()
                )
            }
            Spacer(modifier = Modifier.width(8.dp))
            Column(
                modifier = Modifier.width(104.dp),
                verticalArrangement = Arrangement.Center
            ) {
                Text(
                    text = playerName,
                    color = Color.White,
                    fontSize = 12.sp,
                    fontWeight = FontWeight.ExtraBold,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis
                )
                Spacer(modifier = Modifier.height(2.dp))
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    modifier = Modifier.fillMaxWidth()
                ) {
                    Text(
                        text = "Nv. 67",
                        color = Color(0xFFD5DEEB),
                        fontSize = 10.sp,
                        fontWeight = FontWeight.Bold
                    )
                    Spacer(modifier = Modifier.width(6.dp))
                    Box(
                        modifier = Modifier
                            .weight(1f)
                            .height(5.dp)
                            .clip(RoundedCornerShape(3.dp))
                            .background(Color(0xFF0D111A))
                    ) {
                        Box(
                            modifier = Modifier
                                .fillMaxHeight()
                                .fillMaxWidth(0.62f)
                                .background(ColGold)
                        )
                    }
                }
            }
            Spacer(modifier = Modifier.width(4.dp))
        }

        Spacer(modifier = Modifier.width(4.dp))

        // Barra de Moedas e Diamantes
        Row(
            modifier = Modifier
                .height(38.dp)
                .clip(RoundedCornerShape(6.dp))
                .background(Color(0xCC0F1420))
                .border(1.dp, ColBorder, RoundedCornerShape(6.dp))
                .padding(horizontal = 10.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Canvas(modifier = Modifier.size(18.dp)) {
                drawCoinIcon()
            }
            Spacer(modifier = Modifier.width(6.dp))
            Text(
                text = "205243",
                color = Color.White,
                fontSize = 13.sp,
                fontWeight = FontWeight.ExtraBold
            )
            Spacer(modifier = Modifier.width(14.dp))
            Canvas(modifier = Modifier.size(18.dp)) {
                drawDiamondIcon()
            }
            Spacer(modifier = Modifier.width(6.dp))
            Text(
                text = "6",
                color = Color.White,
                fontSize = 13.sp,
                fontWeight = FontWeight.ExtraBold
            )
            Spacer(modifier = Modifier.width(10.dp))
            Box(
                modifier = Modifier
                    .size(22.dp)
                    .clip(RoundedCornerShape(4.dp))
                    .background(Color(0xFF232C3D))
                    .border(1.dp, ColBorderLight, RoundedCornerShape(4.dp))
                    .clickable(onClick = onPlusClick),
                contentAlignment = Alignment.Center
            ) {
                Text(
                    text = "+",
                    color = Color.White,
                    fontSize = 14.sp,
                    fontWeight = FontWeight.Black
                )
            }
        }
    }
}

@Composable
private fun LobbyLeftMenuButton(
    label: String,
    highlighted: Boolean,
    badge: Boolean,
    icon: DrawScope.() -> Unit,
    onClick: () -> Unit,
) {
    val shape = RoundedCornerShape(6.dp)
    val bgBrush = if (highlighted) {
        Brush.horizontalGradient(listOf(Color(0xE63D2A0A), Color(0xCC1E170C)))
    } else {
        Brush.horizontalGradient(listOf(Color(0xD9171E2E), Color(0xCC111622)))
    }
    val borderColor = if (highlighted) ColGold else ColBorder
    val textColor = if (highlighted) ColGoldBright else Color.White

    Row(
        modifier = Modifier
            .size(width = 122.dp, height = 40.dp)
            .clip(shape)
            .background(bgBrush)
            .border(if (highlighted) 1.5.dp else 1.dp, borderColor, shape)
            .clickable(onClick = onClick)
            .padding(horizontal = 10.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Box(modifier = Modifier.size(22.dp), contentAlignment = Alignment.Center) {
            Canvas(modifier = Modifier.fillMaxSize()) {
                icon()
            }
            if (badge) {
                Box(
                    modifier = Modifier
                        .size(8.dp)
                        .align(Alignment.TopEnd)
                        .offset(x = 2.dp, y = (-2).dp)
                        .clip(CircleShape)
                        .background(ColRed)
                        .border(1.dp, Color.White, CircleShape)
                )
            }
        }
        Spacer(modifier = Modifier.width(10.dp))
        Text(
            text = label,
            color = textColor,
            fontSize = 13.sp,
            fontWeight = FontWeight.ExtraBold,
            letterSpacing = 0.5.sp
        )
    }
}

@Composable
private fun BottomLeftVaultCharacterBar(
    onVaultClick: () -> Unit,
    onCharacterClick: () -> Unit,
    modifier: Modifier = Modifier,
) {
    val shape = CutCornerShape(topStart = 4.dp, bottomStart = 4.dp, topEnd = 10.dp, bottomEnd = 4.dp)
    Row(
        modifier = modifier
            .height(40.dp)
            .clip(shape)
            .background(Color(0xD9111622))
            .border(1.dp, ColBorder, shape),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Row(
            modifier = Modifier
                .fillMaxHeight()
                .clickable(onClick = onVaultClick)
                .padding(horizontal = 16.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Canvas(modifier = Modifier.size(20.dp)) {
                drawChestIcon()
            }
            Spacer(modifier = Modifier.width(8.dp))
            Text(
                text = "COFRE",
                color = Color.White,
                fontSize = 12.sp,
                fontWeight = FontWeight.ExtraBold,
                letterSpacing = 0.5.sp
            )
        }
        Box(
            modifier = Modifier
                .width(1.dp)
                .height(20.dp)
                .background(ColBorderLight)
        )
        Row(
            modifier = Modifier
                .fillMaxHeight()
                .clickable(onClick = onCharacterClick)
                .padding(horizontal = 16.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Canvas(modifier = Modifier.size(19.dp)) {
                drawShirtIcon(Color.White)
            }
            Spacer(modifier = Modifier.width(8.dp))
            Text(
                text = "PERSONAGEM",
                color = Color.White,
                fontSize = 12.sp,
                fontWeight = FontWeight.ExtraBold,
                letterSpacing = 0.5.sp
            )
        }
    }
}

@Composable
private fun RightMatchmakingPanel(
    statusText: String,
    teamSize: Int,
    searching: Boolean,
    onSelectMode: (Int) -> Unit,
    onOpenTeamModal: () -> Unit,
    onPlayClick: () -> Unit,
    modifier: Modifier = Modifier,
) {
    val cardWidth = 236.dp
    Column(
        modifier = modifier.width(cardWidth),
        horizontalAlignment = Alignment.End
    ) {
        // Card BR RANQUEADO + Banner + SOLO / DUO / SQUAD
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .clip(RoundedCornerShape(7.dp))
                .background(Color(0xE6111724))
                .border(1.dp, ColBorderLight, RoundedCornerShape(7.dp))
        ) {
            // Cabeçalho do Card: Insígnia + BR RANQUEADO / Mapa aleatório + Botão troca
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 8.dp, vertical = 6.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Canvas(modifier = Modifier.size(34.dp)) {
                    drawRankCrestIcon()
                }
                Spacer(modifier = Modifier.width(8.dp))
                Column(modifier = Modifier.weight(1f)) {
                    Text(
                        text = "BR RANQUEADO",
                        color = ColGoldBright,
                        fontSize = 13.sp,
                        fontWeight = FontWeight.Black,
                        letterSpacing = 0.4.sp
                    )
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Canvas(modifier = Modifier.size(10.dp)) {
                            drawCircle(ColCyan, radius = size.minDimension * 0.36f, center = Offset(size.width * 0.5f, size.height * 0.4f))
                        }
                        Spacer(modifier = Modifier.width(3.dp))
                        Text(
                            text = if (searching) statusText else "Mapa aleatório",
                            color = Color(0xFF9AC0E6),
                            fontSize = 10.sp,
                            fontWeight = FontWeight.SemiBold,
                            maxLines = 1,
                            overflow = TextOverflow.Ellipsis
                        )
                    }
                }
                Box(
                    modifier = Modifier
                        .size(26.dp)
                        .clip(RoundedCornerShape(5.dp))
                        .background(Color(0xFF1B2336))
                        .border(1.dp, ColBorderLight, RoundedCornerShape(5.dp))
                        .clickable(onClick = onOpenTeamModal),
                    contentAlignment = Alignment.Center
                ) {
                    Canvas(modifier = Modifier.size(14.dp)) {
                        drawSwapArrowsIcon(Color.White)
                    }
                }
            }

            // Faixa ilustrada do mapa (estilo voxel da imagem)
            Canvas(
                modifier = Modifier
                    .fillMaxWidth()
                    .height(40.dp)
            ) {
                drawMapPreviewBanner()
            }

            // Abas SOLO | DUO | SQUAD
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(4.dp),
                horizontalArrangement = Arrangement.spacedBy(4.dp)
            ) {
                ModeTabItem(
                    label = "SOLO",
                    peopleCount = 1,
                    selected = teamSize == 1,
                    onClick = { onSelectMode(1) },
                    modifier = Modifier.weight(1f)
                )
                ModeTabItem(
                    label = "DUO",
                    peopleCount = 2,
                    selected = teamSize == 2,
                    onClick = { onSelectMode(2) },
                    modifier = Modifier.weight(1f)
                )
                ModeTabItem(
                    label = "SQUAD",
                    peopleCount = 3,
                    selected = teamSize == 4,
                    onClick = { onSelectMode(4) },
                    modifier = Modifier.weight(1f)
                )
            }
        }

        Spacer(modifier = Modifier.height(7.dp))

        // Botão amarelo grande: ▶ JOGAR
        TacticalPlayButton(
            searching = searching,
            onClick = onPlayClick,
            modifier = Modifier
                .fillMaxWidth()
                .height(50.dp)
        )
    }
}

@Composable
private fun ModeTabItem(
    label: String,
    peopleCount: Int,
    selected: Boolean,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
) {
    val shape = RoundedCornerShape(5.dp)
    val bg = if (selected) {
        Brush.verticalGradient(listOf(Color(0xFF3A2B10), Color(0xFF241B0A)))
    } else {
        Brush.verticalGradient(listOf(Color(0xFF1A2233), Color(0xFF131926)))
    }
    val borderCol = if (selected) ColGold else Color(0xFF323E57)
    val contentCol = if (selected) ColGoldBright else Color.White

    Column(
        modifier = modifier
            .height(40.dp)
            .clip(shape)
            .background(bg)
            .border(if (selected) 1.5.dp else 1.dp, borderCol, shape)
            .clickable(onClick = onClick),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.Center
    ) {
        Canvas(modifier = Modifier.size(width = 24.dp, height = 13.dp)) {
            drawTeamIcon(Color.White, peopleCount)
        }
        Spacer(modifier = Modifier.height(2.dp))
        Text(
            text = label,
            color = contentCol,
            fontSize = 10.sp,
            fontWeight = FontWeight.ExtraBold
        )
    }
}

@Composable
private fun TacticalPlayButton(
    searching: Boolean,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
) {
    val shape = CutCornerShape(6.dp)
    Box(
        modifier = modifier
            .clip(shape)
            .background(
                Brush.verticalGradient(
                    if (searching) listOf(Color(0xFFFF6B4A), Color(0xFFD9381E))
                    else listOf(ColGoldBright, ColGold, ColGoldDeep)
                )
            )
            .border(1.5.dp, Color(0xFF2A1D04), shape)
            .clickable(onClick = onClick),
        contentAlignment = Alignment.Center
    ) {
        Canvas(modifier = Modifier.fillMaxSize()) {
            // Detalhes táticos nas bordas como na imagem de referência
            val dark = Color(0x44000000)
            drawRect(dark, topLeft = Offset(0f, 0f), size = Size(size.width, 2.dp.toPx()))
            drawRect(dark, topLeft = Offset(0f, size.height - 3.dp.toPx()), size = Size(size.width, 3.dp.toPx()))
            drawLine(Color(0x88FFF6B0), Offset(8.dp.toPx(), 3.dp.toPx()), Offset(size.width - 8.dp.toPx(), 3.dp.toPx()), strokeWidth = 1.5f)
        }
        Row(
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.Center
        ) {
            Canvas(modifier = Modifier.size(18.dp)) {
                val p = Path().apply {
                    moveTo(size.width * 0.15f, size.height * 0.1f)
                    lineTo(size.width * 0.9f, size.height * 0.5f)
                    lineTo(size.width * 0.15f, size.height * 0.9f)
                    close()
                }
                drawPath(p, Color(0xFF14110A))
            }
            Spacer(modifier = Modifier.width(10.dp))
            Text(
                text = if (searching) "CANCELAR" else "JOGAR",
                color = Color(0xFF14110A),
                fontSize = 22.sp,
                fontWeight = FontWeight.Black,
                letterSpacing = 1.sp
            )
        }
    }
}

// ----------------------------------------------------------------------------- Ícones Vetoriais do Lobby

private fun DrawScope.drawCoinIcon() {
    val r = size.minDimension * 0.46f
    drawCircle(Color(0xFFD98200), radius = r)
    drawCircle(Color(0xFFFFC82C), radius = r * 0.82f)
    drawCircle(Color(0xFFD98200), radius = r * 0.65f, style = Stroke(width = size.minDimension * 0.08f))
    // Símbolo $ central
    val cx = size.width * 0.5f
    val cy = size.height * 0.5f
    drawLine(Color(0xFF8C4B00), Offset(cx, cy - r * 0.48f), Offset(cx, cy + r * 0.48f), strokeWidth = size.minDimension * 0.12f)
}

private fun DrawScope.drawDiamondIcon() {
    val w = size.width
    val h = size.height
    val path = Path().apply {
        moveTo(w * 0.22f, h * 0.18f)
        lineTo(w * 0.78f, h * 0.18f)
        lineTo(w * 0.96f, h * 0.42f)
        lineTo(w * 0.50f, h * 0.88f)
        lineTo(w * 0.04f, h * 0.42f)
        close()
    }
    drawPath(path, Color(0xFF1DA8FF))
    val topFacet = Path().apply {
        moveTo(w * 0.22f, h * 0.18f)
        lineTo(w * 0.78f, h * 0.18f)
        lineTo(w * 0.68f, h * 0.42f)
        lineTo(w * 0.32f, h * 0.42f)
        close()
    }
    drawPath(topFacet, Color(0xFF8BE0FF))
}

private fun DrawScope.drawGearIcon(color: Color) {
    val cx = size.width / 2f
    val cy = size.height / 2f
    val rOuter = size.minDimension * 0.44f
    val rInner = size.minDimension * 0.28f
    for (i in 0 until 8) {
        val angle = i * (Math.PI / 4.0)
        val x1 = cx + (rInner * 0.85f * cos(angle)).toFloat()
        val y1 = cy + (rInner * 0.85f * kotlin.math.sin(angle)).toFloat()
        val x2 = cx + (rOuter * cos(angle)).toFloat()
        val y2 = cy + (rOuter * kotlin.math.sin(angle)).toFloat()
        drawLine(color, Offset(x1, y1), Offset(x2, y2), strokeWidth = size.minDimension * 0.16f, cap = StrokeCap.Round)
    }
    drawCircle(color, radius = rInner, style = Stroke(width = size.minDimension * 0.16f))
}

private fun DrawScope.drawCartIcon(color: Color) {
    val w = size.width
    val h = size.height
    val stroke = w * 0.11f
    val path = Path().apply {
        moveTo(w * 0.08f, h * 0.20f)
        lineTo(w * 0.24f, h * 0.20f)
        lineTo(w * 0.36f, h * 0.64f)
        lineTo(w * 0.82f, h * 0.64f)
        lineTo(w * 0.90f, h * 0.32f)
        lineTo(w * 0.28f, h * 0.32f)
    }
    drawPath(path, color, style = Stroke(width = stroke, cap = StrokeCap.Round, join = StrokeJoin.Round))
    drawCircle(color, radius = w * 0.09f, center = Offset(w * 0.42f, h * 0.82f))
    drawCircle(color, radius = w * 0.09f, center = Offset(w * 0.76f, h * 0.82f))
}

private fun DrawScope.drawPassBadgeIcon(color: Color) {
    val w = size.width
    val h = size.height
    val shield = Path().apply {
        moveTo(w * 0.50f, h * 0.08f)
        lineTo(w * 0.86f, h * 0.22f)
        lineTo(w * 0.82f, h * 0.60f)
        lineTo(w * 0.50f, h * 0.92f)
        lineTo(w * 0.18f, h * 0.60f)
        lineTo(w * 0.14f, h * 0.22f)
        close()
    }
    drawPath(shield, color, style = Stroke(width = w * 0.10f, join = StrokeJoin.Round))
    drawCircle(color, radius = w * 0.15f, center = Offset(w * 0.50f, h * 0.48f))
}

private fun DrawScope.drawMissionsIcon(color: Color) {
    val w = size.width
    val h = size.height
    drawRoundRect(
        color = color,
        topLeft = Offset(w * 0.18f, h * 0.16f),
        size = Size(w * 0.64f, h * 0.74f),
        cornerRadius = CornerRadius(w * 0.08f),
        style = Stroke(width = w * 0.10f)
    )
    drawRoundRect(
        color = color,
        topLeft = Offset(w * 0.35f, h * 0.08f),
        size = Size(w * 0.30f, h * 0.14f),
        cornerRadius = CornerRadius(w * 0.04f)
    )
    val check = Path().apply {
        moveTo(w * 0.32f, h * 0.54f)
        lineTo(w * 0.45f, h * 0.68f)
        lineTo(w * 0.68f, h * 0.40f)
    }
    drawPath(check, color, style = Stroke(width = w * 0.11f, cap = StrokeCap.Round, join = StrokeJoin.Round))
}

private fun DrawScope.drawCalendarIcon(color: Color) {
    val w = size.width
    val h = size.height
    drawRoundRect(
        color = color,
        topLeft = Offset(w * 0.14f, h * 0.20f),
        size = Size(w * 0.72f, h * 0.68f),
        cornerRadius = CornerRadius(w * 0.08f),
        style = Stroke(width = w * 0.10f)
    )
    drawLine(color, Offset(w * 0.14f, h * 0.38f), Offset(w * 0.86f, h * 0.38f), strokeWidth = w * 0.09f)
    drawLine(color, Offset(w * 0.32f, h * 0.10f), Offset(w * 0.32f, h * 0.26f), strokeWidth = w * 0.10f, cap = StrokeCap.Round)
    drawLine(color, Offset(w * 0.68f, h * 0.10f), Offset(w * 0.68f, h * 0.26f), strokeWidth = w * 0.10f, cap = StrokeCap.Round)
    for (r in 0..1) {
        for (c in 0..2) {
            drawRect(
                color = color,
                topLeft = Offset(w * (0.26f + c * 0.19f), h * (0.48f + r * 0.17f)),
                size = Size(w * 0.10f, h * 0.10f)
            )
        }
    }
}

private fun DrawScope.drawTeamIcon(color: Color, count: Int) {
    val w = size.width
    val h = size.height
    val positions = when (count) {
        1 -> listOf(0.50f)
        2 -> listOf(0.35f, 0.65f)
        else -> listOf(0.24f, 0.50f, 0.76f)
    }
    positions.forEachIndexed { idx, cx ->
        val primary = count == 1 || (count == 3 && idx == 1) || (count == 2 && idx == 0)
        val scale = if (primary) 1f else 0.86f
        val headY = h * (if (primary) 0.30f else 0.35f)
        val headR = min(w, h) * 0.18f * scale
        drawCircle(color, radius = headR, center = Offset(w * cx, headY))
        val bodyW = headR * 2.1f
        val bodyH = h * 0.36f * scale
        drawRoundRect(
            color = color,
            topLeft = Offset(w * cx - bodyW / 2f, headY + headR * 1.15f),
            size = Size(bodyW, bodyH),
            cornerRadius = CornerRadius(headR * 0.8f)
        )
    }
}

private fun DrawScope.drawChestIcon() {
    val w = size.width
    val h = size.height
    drawRoundRect(
        color = Color(0xFFD98A2B),
        topLeft = Offset(w * 0.10f, h * 0.22f),
        size = Size(w * 0.80f, h * 0.62f),
        cornerRadius = CornerRadius(w * 0.10f)
    )
    drawRect(
        color = Color(0xFF7A4414),
        topLeft = Offset(w * 0.10f, h * 0.48f),
        size = Size(w * 0.80f, h * 0.10f)
    )
    drawRoundRect(
        color = Color(0xFFFFE57F),
        topLeft = Offset(w * 0.40f, h * 0.42f),
        size = Size(w * 0.20f, h * 0.22f),
        cornerRadius = CornerRadius(w * 0.04f)
    )
}

private fun DrawScope.drawShirtIcon(color: Color) {
    val w = size.width
    val h = size.height
    val path = Path().apply {
        moveTo(w * 0.32f, h * 0.14f)
        lineTo(w * 0.68f, h * 0.14f)
        lineTo(w * 0.92f, h * 0.34f)
        lineTo(w * 0.78f, h * 0.50f)
        lineTo(w * 0.68f, h * 0.40f)
        lineTo(w * 0.68f, h * 0.88f)
        lineTo(w * 0.32f, h * 0.88f)
        lineTo(w * 0.32f, h * 0.40f)
        lineTo(w * 0.22f, h * 0.50f)
        lineTo(w * 0.08f, h * 0.34f)
        close()
    }
    drawPath(path, color)
}

private fun DrawScope.drawRankCrestIcon() {
    val w = size.width
    val h = size.height
    val shield = Path().apply {
        moveTo(w * 0.50f, h * 0.06f)
        lineTo(w * 0.88f, h * 0.20f)
        lineTo(w * 0.78f, h * 0.68f)
        lineTo(w * 0.50f, h * 0.86f)
        lineTo(w * 0.22f, h * 0.68f)
        lineTo(w * 0.12f, h * 0.20f)
        close()
    }
    drawPath(shield, Color(0xFFB82918))
    drawPath(shield, ColGold, style = Stroke(width = w * 0.07f))
    // Asas brancas estilizadas no centro do brasão
    val wings = Path().apply {
        moveTo(w * 0.50f, h * 0.26f)
        lineTo(w * 0.74f, h * 0.22f)
        lineTo(w * 0.62f, h * 0.52f)
        lineTo(w * 0.50f, h * 0.64f)
        lineTo(w * 0.38f, h * 0.52f)
        lineTo(w * 0.26f, h * 0.22f)
        close()
    }
    drawPath(wings, Color.White)
    // 3 estrelas douradas na base
    listOf(0.32f, 0.50f, 0.68f).forEachIndexed { i, sx ->
        drawCircle(ColGoldBright, radius = w * (if (i == 1) 0.08f else 0.065f), center = Offset(w * sx, h * 0.88f))
    }
}

private fun DrawScope.drawSwapArrowsIcon(color: Color) {
    val w = size.width
    val h = size.height
    val s = w * 0.11f
    drawLine(color, Offset(w * 0.15f, h * 0.34f), Offset(w * 0.85f, h * 0.34f), strokeWidth = s, cap = StrokeCap.Round)
    drawLine(color, Offset(w * 0.65f, h * 0.16f), Offset(w * 0.85f, h * 0.34f), strokeWidth = s, cap = StrokeCap.Round)
    drawLine(color, Offset(w * 0.65f, h * 0.52f), Offset(w * 0.85f, h * 0.34f), strokeWidth = s, cap = StrokeCap.Round)

    drawLine(color, Offset(w * 0.85f, h * 0.68f), Offset(w * 0.15f, h * 0.68f), strokeWidth = s, cap = StrokeCap.Round)
    drawLine(color, Offset(w * 0.35f, h * 0.50f), Offset(w * 0.15f, h * 0.68f), strokeWidth = s, cap = StrokeCap.Round)
    drawLine(color, Offset(w * 0.35f, h * 0.86f), Offset(w * 0.15f, h * 0.68f), strokeWidth = s, cap = StrokeCap.Round)
}

private fun DrawScope.drawMapPreviewBanner() {
    val w = size.width
    val h = size.height
    // Céu em gradiente
    drawRect(
        brush = Brush.verticalGradient(
            listOf(Color(0xFF4B84C4), Color(0xFF9AC4E8), Color(0xFFE5BA82))
        )
    )
    // Colinas/terreno ao fundo
    drawRect(Color(0xFF8C623E), topLeft = Offset(0f, h * 0.62f), size = Size(w, h * 0.38f))
    drawRect(Color(0xFF4F7A38), topLeft = Offset(0f, h * 0.72f), size = Size(w, h * 0.28f))
    // Forte central voxel
    drawRect(Color(0xFFC89B68), topLeft = Offset(w * 0.34f, h * 0.44f), size = Size(w * 0.32f, h * 0.32f))
    drawRect(Color(0xFF704E2E), topLeft = Offset(w * 0.43f, h * 0.54f), size = Size(w * 0.12f, h * 0.22f))
    // Torre de vigia à direita
    drawRect(Color(0xFF6B4C32), topLeft = Offset(w * 0.74f, h * 0.18f), size = Size(w * 0.11f, h * 0.58f))
    drawRect(Color(0xFF4A3320), topLeft = Offset(w * 0.71f, h * 0.14f), size = Size(w * 0.17f, h * 0.10f))
    // Árvores em blocos na esquerda e direita
    drawRect(Color(0xFF5A3B22), topLeft = Offset(w * 0.13f, h * 0.48f), size = Size(w * 0.04f, h * 0.28f))
    drawRect(Color(0xFF397028), topLeft = Offset(w * 0.07f, h * 0.26f), size = Size(w * 0.16f, h * 0.26f))
    drawRect(Color(0xFF397028), topLeft = Offset(w * 0.87f, h * 0.34f), size = Size(w * 0.11f, h * 0.24f))
}

// ----------------------------------------------------------------------------- Modais do Lobby e Telas Compose

@Composable
private fun PartyComposeModal(
    app: MainActivity,
    statusText: String,
    teamSize: Int,
    autoFill: Boolean,
    onSelectFormation: (Int, Boolean) -> Unit,
    onDismiss: () -> Unit,
) {
    var joinCode by remember { mutableStateOf("") }
    ComposeModalDialog(title = "EQUIPE & FORMAÇÃO", subtitle = statusText, onDismiss = onDismiss) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            listOf(1 to "SOLO", 2 to "DUO", 4 to "SQUAD").forEach { (sz, label) ->
                val active = teamSize == sz
                Button(
                    onClick = { onSelectFormation(sz, autoFill) },
                    colors = ButtonDefaults.buttonColors(
                        containerColor = if (active) ColGold else Color(0xFF1F293D),
                        contentColor = if (active) Color.Black else Color.White
                    ),
                    modifier = Modifier.weight(1f)
                ) {
                    Text(label, fontWeight = FontWeight.ExtraBold)
                }
            }
        }
        Spacer(modifier = Modifier.height(10.dp))
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .clip(RoundedCornerShape(6.dp))
                .background(Color(0xFF182030))
                .clickable { onSelectFormation(teamSize, !autoFill) }
                .padding(horizontal = 12.dp, vertical = 8.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Checkbox(
                checked = autoFill,
                onCheckedChange = { onSelectFormation(teamSize, it) },
                colors = CheckboxDefaults.colors(checkedColor = ColGold, checkmarkColor = Color.Black)
            )
            Spacer(modifier = Modifier.width(8.dp))
            Text("Completar equipe automaticamente", color = Color.White, fontSize = 13.sp, fontWeight = FontWeight.SemiBold)
        }
        Spacer(modifier = Modifier.height(10.dp))
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            Button(
                onClick = { app.group("create") },
                colors = ButtonDefaults.buttonColors(containerColor = ColCyan, contentColor = Color.Black),
                modifier = Modifier.weight(1f)
            ) {
                Text("CRIAR GRUPO", fontWeight = FontWeight.ExtraBold, fontSize = 12.sp)
            }
            Button(
                onClick = { app.group("leave") },
                colors = ButtonDefaults.buttonColors(containerColor = Color(0xFF28334A), contentColor = Color.White),
                modifier = Modifier.weight(1f)
            ) {
                Text("SAIR DO GRUPO", fontWeight = FontWeight.ExtraBold, fontSize = 12.sp)
            }
        }
        Spacer(modifier = Modifier.height(10.dp))
        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            OutlinedTextField(
                value = joinCode,
                onValueChange = { if (it.length <= 8) joinCode = it.uppercase() },
                label = { Text("Código do grupo") },
                singleLine = true,
                modifier = Modifier.weight(1f)
            )
            Button(
                onClick = { if (joinCode.isNotBlank()) app.group("join", joinCode) },
                colors = ButtonDefaults.buttonColors(containerColor = ColGold, contentColor = Color.Black)
            ) {
                Text("ENTRAR", fontWeight = FontWeight.ExtraBold)
            }
        }
    }
}

@Composable
private fun CharacterWeaponComposeModal(
    app: MainActivity,
    selectedWeapon: Int,
    onDismiss: () -> Unit,
) {
    var current by remember { mutableIntStateOf(selectedWeapon) }
    val descriptions = listOf(
        "12 tiros • munição de pistola",
        "30 tiros • munição de SMG",
        "30 tiros • munição de fuzil",
        "8 tiros • cartuchos",
        "5 tiros • munição de precisão",
        "Corpo a corpo"
    )
    ComposeModalDialog(
        title = "COFRE & PERSONAGEM",
        subtitle = "ESCOLHA A ARMA SEGURADA NO LOBBY",
        onDismiss = onDismiss
    ) {
        Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
            for (id in 1..6) {
                val active = current == id
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .clip(RoundedCornerShape(6.dp))
                        .background(if (active) Color(0xFF3B2B0E) else Color(0xFF182030))
                        .border(1.dp, if (active) ColGold else ColBorder, RoundedCornerShape(6.dp))
                        .clickable {
                            current = id
                            app.progress.settings.previewWeapon = id
                            app.save()
                            app.engine { NativeBridge.previewWeapon(id) }
                        }
                        .padding(horizontal = 12.dp, vertical = 10.dp),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Column(modifier = Modifier.weight(1f)) {
                        Text(
                            text = BattleItems.name(id),
                            color = if (active) ColGoldBright else Color.White,
                            fontWeight = FontWeight.ExtraBold,
                            fontSize = 14.sp
                        )
                        Text(
                            text = descriptions[id - 1],
                            color = ColMuted,
                            fontSize = 11.sp
                        )
                    }
                    Text(
                        text = if (active) "EM MÃOS ✓" else "EQUIPAR",
                        color = if (active) ColGoldBright else ColCyan,
                        fontWeight = FontWeight.ExtraBold,
                        fontSize = 12.sp
                    )
                }
            }
        }
    }
}

@Composable
private fun ComposeModalDialog(
    title: String,
    subtitle: String,
    onDismiss: () -> Unit,
    content: @Composable ColumnScope.() -> Unit,
) {
    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(Color(0xB8050811))
            .clickable(
                interactionSource = remember { MutableInteractionSource() },
                indication = null,
                onClick = onDismiss
            ),
        contentAlignment = Alignment.Center
    ) {
        Column(
            modifier = Modifier
                .widthIn(max = 460.dp)
                .fillMaxWidth(0.86f)
                .heightIn(max = 340.dp)
                .clip(RoundedCornerShape(10.dp))
                .background(Color(0xF2101624))
                .border(1.5.dp, ColBorderLight, RoundedCornerShape(10.dp))
                .clickable(
                    interactionSource = remember { MutableInteractionSource() },
                    indication = null,
                    onClick = {}
                )
                .padding(16.dp)
        ) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Column(modifier = Modifier.weight(1f)) {
                    Text(subtitle, color = ColGold, fontSize = 10.sp, fontWeight = FontWeight.ExtraBold)
                    Text(title, color = Color.White, fontSize = 18.sp, fontWeight = FontWeight.Black)
                }
                Box(
                    modifier = Modifier
                        .size(34.dp)
                        .clip(RoundedCornerShape(6.dp))
                        .background(Color(0xFF232C3F))
                        .clickable(onClick = onDismiss),
                    contentAlignment = Alignment.Center
                ) {
                    Text("✕", color = Color.White, fontWeight = FontWeight.Bold, fontSize = 16.sp)
                }
            }
            Spacer(modifier = Modifier.height(12.dp))
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .verticalScroll(rememberScrollState()),
                content = content
            )
        }
    }
}

// ----------------------------------------------------------------------------- Tela de Armas (Cofre) em Compose

@Composable
fun ArmoryComposeScreen(
    app: MainActivity,
    selectedWeapon: Int,
    onSelectWeapon: (Int) -> Unit,
    onBack: () -> Unit,
) {
    val descriptions = listOf(
        "12 tiros • munição de pistola",
        "30 tiros • munição de SMG",
        "30 tiros • munição de fuzil",
        "8 tiros • cartuchos",
        "5 tiros • munição de precisão",
        "Corpo a corpo"
    )
    Box(modifier = Modifier.fillMaxSize()) {
        Column(
            modifier = Modifier
                .width(340.dp)
                .fillMaxHeight()
                .background(Color(0xD90A101D))
                .border(1.dp, ColBorder)
                .padding(16.dp)
        ) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Column(modifier = Modifier.weight(1f)) {
                    Text("PRÉVIA NO LOBBY", color = ColGold, fontSize = 10.sp, fontWeight = FontWeight.ExtraBold)
                    Text("COFRE DE ARMAS", color = Color.White, fontSize = 20.sp, fontWeight = FontWeight.Black)
                }
                Box(
                    modifier = Modifier
                        .size(38.dp)
                        .clip(RoundedCornerShape(6.dp))
                        .background(Color(0xFF222B3D))
                        .clickable(onClick = onBack),
                    contentAlignment = Alignment.Center
                ) {
                    Text("✕", color = Color.White, fontSize = 16.sp, fontWeight = FontWeight.Bold)
                }
            }
            Spacer(modifier = Modifier.height(6.dp))
            Text(
                "Escolha a arma que o personagem segura no lobby. Na partida, encontre armas pelo mapa.",
                color = ColMuted,
                fontSize = 11.sp
            )
            Spacer(modifier = Modifier.height(10.dp))
            Column(
                modifier = Modifier
                    .weight(1f)
                    .verticalScroll(rememberScrollState()),
                verticalArrangement = Arrangement.spacedBy(6.dp)
            ) {
                for (id in 1..6) {
                    val active = selectedWeapon == id
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .clip(RoundedCornerShape(6.dp))
                            .background(if (active) Color(0xFF3D2C0D) else Color(0xFF171F30))
                            .border(1.5.dp, if (active) ColGold else ColBorder, RoundedCornerShape(6.dp))
                            .clickable { onSelectWeapon(id) }
                            .padding(horizontal = 12.dp, vertical = 10.dp),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Column(modifier = Modifier.weight(1f)) {
                            Text(
                                BattleItems.name(id),
                                color = if (active) ColGoldBright else Color.White,
                                fontSize = 14.sp,
                                fontWeight = FontWeight.ExtraBold
                            )
                            Text(descriptions[id - 1], color = ColMuted, fontSize = 11.sp)
                        }
                        if (active) {
                            Text("EQUIPADA", color = ColGoldBright, fontSize = 11.sp, fontWeight = FontWeight.Black)
                        }
                    }
                }
            }
        }
    }
}

// ----------------------------------------------------------------------------- Tela de Ajustes em Compose

@Composable
fun SettingsComposeScreen(
    app: MainActivity,
    fromPause: Boolean,
    onOpenHudEditor: () -> Unit,
    onClose: () -> Unit,
) {
    val s = app.progress.settings
    var sensitivity by remember { mutableFloatStateOf(s.sensitivity) }
    var invertY by remember { mutableStateOf(s.invertY) }
    var leftHanded by remember { mutableStateOf(s.leftHanded) }
    var vibration by remember { mutableStateOf(s.vibration) }
    var musicVol by remember { mutableFloatStateOf(s.musicVolume) }
    var sfxVol by remember { mutableFloatStateOf(s.sfxVolume) }
    var quality by remember { mutableIntStateOf(s.quality) }
    var showFps by remember { mutableStateOf(s.showFps) }

    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(Color(if (fromPause) 0xE0060A14 else 0xCC060A14))
            .clickable(interactionSource = remember { MutableInteractionSource() }, indication = null) {}
            .padding(horizontal = 20.dp, vertical = 12.dp)
    ) {
        Column(modifier = Modifier.fillMaxSize()) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Column(modifier = Modifier.weight(1f)) {
                    Text("CONTROLES • SOM • GRÁFICOS", color = ColGold, fontSize = 10.sp, fontWeight = FontWeight.ExtraBold)
                    Text("AJUSTES", color = Color.White, fontSize = 22.sp, fontWeight = FontWeight.Black)
                }
                Box(
                    modifier = Modifier
                        .size(40.dp)
                        .clip(RoundedCornerShape(6.dp))
                        .background(Color(0xFF222B3D))
                        .border(1.dp, ColBorderLight, RoundedCornerShape(6.dp))
                        .clickable {
                            app.applySettings()
                            onClose()
                        },
                    contentAlignment = Alignment.Center
                ) {
                    Text("✕", color = Color.White, fontSize = 16.sp, fontWeight = FontWeight.Bold)
                }
            }
            Spacer(modifier = Modifier.height(10.dp))
            Row(
                modifier = Modifier
                    .fillMaxSize()
                    .verticalScroll(rememberScrollState()),
                horizontalArrangement = Arrangement.spacedBy(16.dp)
            ) {
                // Coluna Esquerda: Controles
                Column(
                    modifier = Modifier.weight(1f),
                    verticalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    Text("CONTROLES", color = ColGold, fontSize = 12.sp, fontWeight = FontWeight.ExtraBold)
                    SettingSliderCard(
                        label = "Sensibilidade da câmera",
                        value = sensitivity,
                        valueRange = 0.3f..2.5f,
                        onValueChange = {
                            sensitivity = it
                            s.sensitivity = it
                            app.applySettings()
                        }
                    )
                    SettingToggleCard("Inverter eixo vertical", "", invertY) {
                        invertY = it
                        s.invertY = it
                        app.applySettings()
                    }
                    Button(
                        onClick = {
                            app.applySettings()
                            onOpenHudEditor()
                        },
                        colors = ButtonDefaults.buttonColors(containerColor = ColCyan, contentColor = Color.Black),
                        modifier = Modifier
                            .fillMaxWidth()
                            .height(44.dp)
                    ) {
                        Text("✥ PERSONALIZAR CONTROLES (HUD)", fontWeight = FontWeight.ExtraBold, fontSize = 12.sp)
                    }
                    SettingToggleCard("Modo canhoto", "Espelha o layout padrão", leftHanded) {
                        leftHanded = it
                        s.leftHanded = it
                        s.hudLayout = ""
                        app.applySettings()
                    }
                    SettingToggleCard("Vibração", "Ao levar dano e acertar tiros", vibration) {
                        vibration = it
                        s.vibration = it
                        app.applySettings()
                    }
                }

                // Coluna Direita: Som e Gráficos
                Column(
                    modifier = Modifier.weight(1f),
                    verticalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    Text("SOM", color = ColGold, fontSize = 12.sp, fontWeight = FontWeight.ExtraBold)
                    SettingSliderCard(
                        label = "Música",
                        value = musicVol,
                        valueRange = 0f..1f,
                        onValueChange = {
                            musicVol = it
                            s.musicVolume = it
                            app.audio.musicVolume = it
                            app.applySettings()
                        }
                    )
                    SettingSliderCard(
                        label = "Efeitos",
                        value = sfxVol,
                        valueRange = 0f..1f,
                        onValueChange = {
                            sfxVol = it
                            s.sfxVolume = it
                            app.audio.sfxVolume = it
                            app.applySettings()
                        }
                    )
                    Text("GRÁFICOS", color = ColGold, fontSize = 12.sp, fontWeight = FontWeight.ExtraBold)
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.spacedBy(6.dp)
                    ) {
                        listOf("BAIXA", "MÉDIA", "ALTA").forEachIndexed { idx, label ->
                            val active = quality == idx
                            Button(
                                onClick = {
                                    quality = idx
                                    s.quality = idx
                                    app.applySettings()
                                },
                                colors = ButtonDefaults.buttonColors(
                                    containerColor = if (active) ColGold else Color(0xFF1E2638),
                                    contentColor = if (active) Color.Black else Color.White
                                ),
                                modifier = Modifier
                                    .weight(1f)
                                    .height(40.dp)
                            ) {
                                Text(label, fontSize = 11.sp, fontWeight = FontWeight.ExtraBold)
                            }
                        }
                    }
                    SettingToggleCard("Mostrar FPS", "", showFps) {
                        showFps = it
                        s.showFps = it
                        app.applySettings()
                    }
                }
            }
        }
    }
}

@Composable
private fun SettingSliderCard(
    label: String,
    value: Float,
    valueRange: ClosedFloatingPointRange<Float>,
    onValueChange: (Float) -> Unit,
) {
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(6.dp))
            .background(Color(0xFF151C2C))
            .border(1.dp, ColBorder, RoundedCornerShape(6.dp))
            .padding(horizontal = 12.dp, vertical = 8.dp)
    ) {
        Text(label, color = Color.White, fontSize = 13.sp, fontWeight = FontWeight.Bold)
        Slider(
            value = value,
            onValueChange = onValueChange,
            valueRange = valueRange,
            colors = SliderDefaults.colors(thumbColor = ColGold, activeTrackColor = ColGold)
        )
    }
}

@Composable
private fun SettingToggleCard(
    label: String,
    hint: String,
    checked: Boolean,
    onCheckedChange: (Boolean) -> Unit,
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(6.dp))
            .background(Color(0xFF151C2C))
            .border(1.dp, ColBorder, RoundedCornerShape(6.dp))
            .clickable { onCheckedChange(!checked) }
            .padding(horizontal = 12.dp, vertical = 8.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Column(modifier = Modifier.weight(1f)) {
            Text(label, color = Color.White, fontSize = 13.sp, fontWeight = FontWeight.Bold)
            if (hint.isNotEmpty()) {
                Text(hint, color = ColMuted, fontSize = 10.sp)
            }
        }
        Switch(
            checked = checked,
            onCheckedChange = onCheckedChange,
            colors = SwitchDefaults.colors(checkedThumbColor = Color.Black, checkedTrackColor = ColGold)
        )
    }
}

// ----------------------------------------------------------------------------- Editor de HUD em Compose

@Composable
fun HudEditorComposeScreen(
    app: MainActivity,
    onClose: () -> Unit,
) {
    val settings = app.progress.settings
    var opacity by remember { mutableFloatStateOf(settings.hudOpacity) }
    var selectedName by remember { mutableStateOf("Toque num controle") }
    var canHide by remember { mutableStateOf(false) }
    var isHidden by remember { mutableStateOf(false) }
    val editorHud = remember {
        HudView(app, object : HudView.Listener {
            override fun onPauseRequested() = Unit
            override fun onMapRequested() = Unit
        }).apply {
            applySettings(settings.leftHanded, false, false, settings.hudLayout, settings.hudOpacity)
            editing = true
        }
    }

    fun syncSelection() {
        val c = editorHud.selected
        selectedName = c?.label?.uppercase() ?: "Toque num controle"
        canHide = c?.optional == true
        isHidden = canHide && !editorHud.layout[c!!].visible
    }

    DisposableEffect(editorHud) {
        editorHud.onSelectionChanged = { syncSelection() }
        syncSelection()
        onDispose { editorHud.onSelectionChanged = null }
    }

    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(Color(0x9904070E))
    ) {
        AndroidView(
            factory = { editorHud },
            modifier = Modifier.fillMaxSize()
        )
        Column(
            modifier = Modifier
                .align(Alignment.TopCenter)
                .padding(top = 8.dp)
                .clip(RoundedCornerShape(8.dp))
                .background(Color(0xEE0E1628))
                .border(1.dp, ColBorderLight, RoundedCornerShape(8.dp))
                .padding(10.dp),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            Row(
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(6.dp)
            ) {
                Text(
                    text = selectedName,
                    color = ColGoldBright,
                    fontSize = 12.sp,
                    fontWeight = FontWeight.ExtraBold,
                    modifier = Modifier.width(140.dp)
                )
                Button(onClick = { editorHud.scaleSelected(-0.1f) }, contentPadding = PaddingValues(0.dp), modifier = Modifier.size(38.dp, 34.dp)) { Text("−") }
                Text("TAMANHO", color = ColMuted, fontSize = 9.sp, fontWeight = FontWeight.Bold)
                Button(onClick = { editorHud.scaleSelected(0.1f) }, contentPadding = PaddingValues(0.dp), modifier = Modifier.size(38.dp, 34.dp)) { Text("+") }
                Button(
                    onClick = {
                        editorHud.toggleSelectedVisible()
                        syncSelection()
                    },
                    enabled = canHide,
                    modifier = Modifier.height(34.dp)
                ) {
                    Text(if (isHidden) "MOSTRAR" else "OCULTAR", fontSize = 11.sp)
                }
                Button(
                    onClick = {
                        editorHud.setWorkingLayout(editorHud.defaultLayout())
                        syncSelection()
                    },
                    modifier = Modifier.height(34.dp)
                ) {
                    Text("PADRÃO", fontSize = 11.sp)
                }
                Button(onClick = onClose, modifier = Modifier.height(34.dp)) {
                    Text("CANCELAR", fontSize = 11.sp)
                }
                Button(
                    onClick = {
                        settings.hudLayout = editorHud.layout.encode()
                        settings.hudOpacity = opacity
                        app.applySettings()
                        app.audio.playUi("reward", 0.7f)
                        onClose()
                    },
                    colors = ButtonDefaults.buttonColors(containerColor = ColLime, contentColor = Color.Black),
                    modifier = Modifier.height(34.dp)
                ) {
                    Text("SALVAR ✓", fontSize = 11.sp, fontWeight = FontWeight.ExtraBold)
                }
            }
            Spacer(modifier = Modifier.height(6.dp))
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Text("TRANSPARÊNCIA ${(opacity * 100).toInt()}%", color = Color.White, fontSize = 11.sp, fontWeight = FontWeight.Bold)
                Button(
                    onClick = {
                        opacity = (opacity - 0.1f).coerceIn(0.3f, 1f)
                        editorHud.opacity = opacity
                        editorHud.invalidate()
                    },
                    contentPadding = PaddingValues(0.dp),
                    modifier = Modifier.size(32.dp, 28.dp)
                ) { Text("−") }
                Button(
                    onClick = {
                        opacity = (opacity + 0.1f).coerceIn(0.3f, 1f)
                        editorHud.opacity = opacity
                        editorHud.invalidate()
                    },
                    contentPadding = PaddingValues(0.dp),
                    modifier = Modifier.size(32.dp, 28.dp)
                ) { Text("+") }
            }
        }
    }
}

// ----------------------------------------------------------------------------- Overlays de Partida em Compose

@Composable
fun LoadingComposeOverlay() {
    val transition = rememberInfiniteTransition(label = "loading")
    val phase by transition.animateFloat(
        initialValue = 0f,
        targetValue = 1f,
        animationSpec = infiniteRepeatable(tween(1100, easing = LinearEasing)),
        label = "spinner"
    )
    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(Color.Black)
            .clickable(interactionSource = remember { MutableInteractionSource() }, indication = null) {},
        contentAlignment = Alignment.Center
    ) {
        Column(horizontalAlignment = Alignment.CenterHorizontally) {
            Canvas(modifier = Modifier.size(56.dp)) {
                val stroke = 3.dp.toPx()
                drawArc(Color(0x28FFFFFF), 0f, 360f, false, style = Stroke(stroke))
                val sweep = 50f + 230f * (0.5f - 0.5f * cos(phase * 4f * Math.PI.toFloat()))
                drawArc(Color.White, phase * 360f - 90f, sweep, false, style = Stroke(stroke, cap = StrokeCap.Round))
            }
            Spacer(modifier = Modifier.height(22.dp))
            Text("CARREGANDO", color = Color.White, fontSize = 14.sp, fontWeight = FontWeight.ExtraBold, letterSpacing = 3.sp)
        }
        Text(
            text = "INICIANDO...",
            color = Color(0xAAFFFFFF),
            fontSize = 11.sp,
            fontWeight = FontWeight.Bold,
            modifier = Modifier
                .align(Alignment.BottomCenter)
                .padding(bottom = 36.dp)
        )
    }
}

@Composable
fun PauseComposeOverlay(
    onResume: () -> Unit,
    onSettings: () -> Unit,
    onAbandon: () -> Unit,
) {
    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(Color(0xAA04070E))
            .clickable(interactionSource = remember { MutableInteractionSource() }, indication = null) {},
        contentAlignment = Alignment.Center
    ) {
        Column(
            modifier = Modifier
                .width(320.dp)
                .clip(RoundedCornerShape(10.dp))
                .background(Color(0xF2101624))
                .border(1.5.dp, ColBorderLight, RoundedCornerShape(10.dp))
                .padding(20.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            Text("PAUSADO", color = ColGoldBright, fontSize = 24.sp, fontWeight = FontWeight.Black)
            Text("A partida online continua durante a pausa", color = ColMuted, fontSize = 12.sp, textAlign = TextAlign.Center)
            Spacer(modifier = Modifier.height(4.dp))
            Button(
                onClick = onResume,
                colors = ButtonDefaults.buttonColors(containerColor = ColGold, contentColor = Color.Black),
                modifier = Modifier.fillMaxWidth().height(42.dp)
            ) {
                Text("CONTINUAR ▶", fontWeight = FontWeight.ExtraBold)
            }
            Button(
                onClick = onSettings,
                colors = ButtonDefaults.buttonColors(containerColor = Color(0xFF232C3F), contentColor = Color.White),
                modifier = Modifier.fillMaxWidth().height(42.dp)
            ) {
                Text("AJUSTES", fontWeight = FontWeight.ExtraBold)
            }
            Button(
                onClick = onAbandon,
                colors = ButtonDefaults.buttonColors(containerColor = ColRed, contentColor = Color.White),
                modifier = Modifier.fillMaxWidth().height(42.dp)
            ) {
                Text("VOLTAR AO LOBBY", fontWeight = FontWeight.ExtraBold)
            }
        }
    }
}

@Composable
fun ResultComposeOverlay(
    place: Int,
    kills: Int,
    training: Boolean,
    onReturnLobby: () -> Unit,
) {
    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(Color(0xD2050A14))
            .clickable(interactionSource = remember { MutableInteractionSource() }, indication = null) {},
        contentAlignment = Alignment.Center
    ) {
        Column(
            modifier = Modifier
                .width(360.dp)
                .clip(RoundedCornerShape(10.dp))
                .background(Color(0xF2101624))
                .border(1.5.dp, ColGold, RoundedCornerShape(10.dp))
                .padding(22.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.spacedBy(10.dp)
        ) {
            Text(
                text = if (place == 1) "VITÓRIA" else "PARTIDA ENCERRADA",
                color = ColGoldBright,
                fontSize = 26.sp,
                fontWeight = FontWeight.Black
            )
            Text(
                text = "${if (place > 0) "Classificação #$place • " else ""}$kills eliminações${if (training) " • treino" else ""}",
                color = Color.White,
                fontSize = 15.sp,
                fontWeight = FontWeight.Bold
            )
            Spacer(modifier = Modifier.height(6.dp))
            Button(
                onClick = onReturnLobby,
                colors = ButtonDefaults.buttonColors(containerColor = ColGold, contentColor = Color.Black),
                modifier = Modifier.fillMaxWidth().height(46.dp)
            ) {
                Text("VOLTAR AO LOBBY", fontWeight = FontWeight.Black, fontSize = 15.sp)
            }
        }
    }
}

@Composable
fun BackpackComposeOverlay(
    app: MainActivity,
    state: FloatArray,
    onClose: () -> Unit,
) {
    val offset = state.size - 48
    val slots = remember(state) {
        (0 until 24).mapNotNull { slot ->
            val id = state[offset + slot * 2].toInt()
            val count = state[offset + slot * 2 + 1].toInt()
            if (id == 0) null else Triple(slot, id, count)
        }
    }
    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(Color(0xCC050A14))
            .clickable(interactionSource = remember { MutableInteractionSource() }, indication = null, onClick = onClose)
            .padding(20.dp),
        contentAlignment = Alignment.Center
    ) {
        Column(
            modifier = Modifier
                .widthIn(max = 520.dp)
                .fillMaxWidth()
                .fillMaxHeight(0.88f)
                .clip(RoundedCornerShape(10.dp))
                .background(Color(0xF2101624))
                .border(1.5.dp, ColBorderLight, RoundedCornerShape(10.dp))
                .clickable(interactionSource = remember { MutableInteractionSource() }, indication = null) {}
                .padding(16.dp)
        ) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Column(modifier = Modifier.weight(1f)) {
                    Text("EQUIPAMENTO DA PARTIDA", color = ColGold, fontSize = 10.sp, fontWeight = FontWeight.ExtraBold)
                    Text("MOCHILA", color = Color.White, fontSize = 20.sp, fontWeight = FontWeight.Black)
                }
                Box(
                    modifier = Modifier
                        .size(36.dp)
                        .clip(RoundedCornerShape(6.dp))
                        .background(Color(0xFF232C3F))
                        .clickable(onClick = onClose),
                    contentAlignment = Alignment.Center
                ) {
                    Text("✕", color = Color.White, fontWeight = FontWeight.Bold)
                }
            }
            Spacer(modifier = Modifier.height(10.dp))
            if (slots.isEmpty()) {
                Box(modifier = Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                    Text("Mochila vazia", color = ColMuted, fontSize = 14.sp)
                }
            } else {
                LazyColumn(verticalArrangement = Arrangement.spacedBy(6.dp)) {
                    items(slots) { (slot, id, count) ->
                        Row(
                            modifier = Modifier
                                .fillMaxWidth()
                                .clip(RoundedCornerShape(6.dp))
                                .background(Color(0xFF182032))
                                .border(1.dp, ColBorder, RoundedCornerShape(6.dp))
                                .padding(horizontal = 12.dp, vertical = 8.dp),
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Text(
                                text = "${BattleItems.name(id)} × $count",
                                color = Color.White,
                                fontSize = 13.sp,
                                fontWeight = FontWeight.Bold,
                                modifier = Modifier.weight(1f)
                            )
                            Button(
                                onClick = {
                                    when (id) {
                                        27 -> NativeBridge.battleAction(128)
                                        25 -> NativeBridge.battleAction(256)
                                        26 -> NativeBridge.battleAction(512)
                                        23, 24, 28 -> app.onStatus("Segure CURAR no jogo")
                                        else -> NativeBridge.tapSlot(slot)
                                    }
                                    onClose()
                                },
                                colors = ButtonDefaults.buttonColors(containerColor = ColGold, contentColor = Color.Black),
                                modifier = Modifier.height(34.dp)
                            ) {
                                Text(if (id in 1..6 || id in 20..22) "EQUIPAR" else "USAR", fontSize = 11.sp, fontWeight = FontWeight.ExtraBold)
                            }
                            Spacer(modifier = Modifier.width(6.dp))
                            Button(
                                onClick = {
                                    app.engine { NativeBridge.dropItem(id, 1) }
                                    onClose()
                                },
                                colors = ButtonDefaults.buttonColors(containerColor = Color(0xFF28334A), contentColor = Color.White),
                                modifier = Modifier.height(34.dp)
                            ) {
                                Text("SOLTAR 1", fontSize = 11.sp, fontWeight = FontWeight.Bold)
                            }
                        }
                    }
                }
            }
        }
    }
}

@Composable
fun BattleMapComposeOverlay(
    mapBitmap: Bitmap?,
    state: FloatArray,
    localId: Int,
    onMarkAndClose: (Float, Float) -> Unit,
) {
    AndroidView(
        factory = { ctx ->
            BattleMapView(ctx, mapBitmap, state, localId) { x, z ->
                onMarkAndClose(x, z)
            }
        },
        modifier = Modifier.fillMaxSize()
    )
}

@Composable
fun RecoveryComposeScreen(
    app: MainActivity,
    initialReport: RecoveryReport,
) {
    var report by remember { mutableStateOf(initialReport) }
    var dialogMsg by remember { mutableStateOf<Pair<String, String>?>(null) }

    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(Color(0xFF0B101B))
            .padding(24.dp)
    ) {
        Row(
            modifier = Modifier.fillMaxSize(),
            horizontalArrangement = Arrangement.spacedBy(18.dp)
        ) {
            Column(modifier = Modifier.weight(1f)) {
                Text("PROTEÇÃO DO PROGRESSO", color = ColGold, fontSize = 10.sp, fontWeight = FontWeight.ExtraBold)
                Text("NÃO FOI POSSÍVEL ABRIR O SAVE", color = ColRed, fontSize = 20.sp, fontWeight = FontWeight.Black)
                Spacer(modifier = Modifier.height(8.dp))
                Text(
                    text = "Seu progresso NÃO foi apagado: os arquivos foram mantidos sem alteração. Escolha uma opção ao lado.",
                    color = Color.White,
                    fontSize = 13.sp
                )
            }
            Column(
                modifier = Modifier.weight(1f),
                verticalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                Button(
                    onClick = {
                        when (val r = app.store.load()) {
                            is LoadResult.Loaded -> app.onRecoveryResolved(r.progress, r.notices)
                            is LoadResult.Fresh -> app.onRecoveryResolved(app.store.startNewGame())
                            is LoadResult.NeedsRecovery -> report = r.report
                        }
                    },
                    colors = ButtonDefaults.buttonColors(containerColor = ColLime, contentColor = Color.Black),
                    modifier = Modifier.fillMaxWidth()
                ) {
                    Text("TENTAR DE NOVO", fontWeight = FontWeight.ExtraBold)
                }
                report.restorable.forEach { b: BackupInfo ->
                    Button(
                        onClick = { app.onRecoveryResolved(app.store.restore(b)) },
                        colors = ButtonDefaults.buttonColors(containerColor = ColCyan, contentColor = Color.Black),
                        modifier = Modifier.fillMaxWidth()
                    ) {
                        val dt = SimpleDateFormat("dd/MM HH:mm", Locale.forLanguageTag("pt-BR")).format(Date(b.modifiedAt))
                        Text("RESTAURAR: ${b.label.uppercase()} ($dt)", fontWeight = FontWeight.ExtraBold, fontSize = 11.sp)
                    }
                }
                Button(
                    onClick = { app.onRecoveryResolved(app.store.startNewGame()) },
                    colors = ButtonDefaults.buttonColors(containerColor = ColGold, contentColor = Color.Black),
                    modifier = Modifier.fillMaxWidth()
                ) {
                    Text("COMEÇAR JOGO NOVO", fontWeight = FontWeight.ExtraBold)
                }
                Button(
                    onClick = { app.finish() },
                    colors = ButtonDefaults.buttonColors(containerColor = Color(0xFF232C3F), contentColor = Color.White),
                    modifier = Modifier.fillMaxWidth()
                ) {
                    Text("FECHAR O JOGO", fontWeight = FontWeight.Bold)
                }
            }
        }
        dialogMsg?.let { (t, m) ->
            ComposeModalDialog(title = t, subtitle = "AVISO", onDismiss = { dialogMsg = null }) {
                Text(m, color = Color.White, fontSize = 13.sp)
            }
        }
    }
}

