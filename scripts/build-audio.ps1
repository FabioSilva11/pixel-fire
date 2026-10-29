# Gera os WAVs sintetizados e converte para OGG Vorbis em app/src/main/res/raw.
# Requer Node.js e ffmpeg (com libvorbis) no PATH.
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$wav = Join-Path $root 'build\audio-wav'
$raw = Join-Path $root 'app\src\main\res\raw'
New-Item -ItemType Directory -Force -Path $wav, $raw | Out-Null

node (Join-Path $PSScriptRoot 'generate-audio.mjs') $wav
if ($LASTEXITCODE -ne 0) { throw 'Falha ao sintetizar o áudio.' }

Get-ChildItem $wav -Filter *.wav | ForEach-Object {
    $out = Join-Path $raw ($_.BaseName + '.ogg')
    $quality = if ($_.BaseName -like 'music_*') { '3' } else { '4' }
    ffmpeg -y -loglevel error -i $_.FullName -c:a libvorbis -q:a $quality $out
    if ($LASTEXITCODE -ne 0) { throw "ffmpeg falhou em $($_.Name)" }
}
Get-ChildItem $raw -Filter *.ogg | Select-Object Name, Length
