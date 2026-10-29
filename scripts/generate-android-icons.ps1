[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$Source
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$sourcePath = [System.IO.Path]::GetFullPath($Source)
if (-not [System.IO.File]::Exists($sourcePath)) {
    throw "Source PNG was not found: $sourcePath"
}

$resRoot = Join-Path $PSScriptRoot '..\app\src\main\res'
$sizes = [ordered]@{ mdpi = 48; hdpi = 72; xhdpi = 96; xxhdpi = 144; xxxhdpi = 192 }
$background = [System.Drawing.ColorTranslator]::FromHtml('#08111f')
$pngFormat = [System.Drawing.Imaging.ImageFormat]::Png

function New-CanvasBitmap([int]$edge, [bool]$transparent) {
    $bitmap = New-Object System.Drawing.Bitmap($edge, $edge, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        if ($transparent) {
            $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
            $graphics.Clear([System.Drawing.Color]::Transparent)
            $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceOver
        } else {
            $graphics.Clear($background)
        }
    } finally {
        $graphics.Dispose()
    }
    return $bitmap
}

function Draw-Foreground([System.Drawing.Graphics]$graphics, [System.Drawing.Image]$foreground, [int]$edge, [double]$occupancy) {
    $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
    $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality

    $drawEdge = [int][Math]::Round($edge * $occupancy, [System.MidpointRounding]::AwayFromZero)
    $offset = [int][Math]::Floor(($edge - $drawEdge) / 2)
    $destination = New-Object System.Drawing.Rectangle($offset, $offset, $drawEdge, $drawEdge)
    $graphics.DrawImage($foreground, $destination)
}

function Save-Icon([System.Drawing.Image]$foreground, [int]$edge, [string]$path, [bool]$round) {
    $bitmap = New-CanvasBitmap $edge $round
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        if ($round) {
            $clip = New-Object System.Drawing.Drawing2D.GraphicsPath
            try {
                $clip.AddEllipse(0, 0, $edge, $edge)
                $graphics.SetClip($clip)
                $graphics.Clear($background)
                Draw-Foreground $graphics $foreground $edge 0.76
            } finally {
                $clip.Dispose()
            }
        } else {
            Draw-Foreground $graphics $foreground $edge 0.76
        }
        $bitmap.Save($path, $pngFormat)
    } finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

$sourceBitmap = [System.Drawing.Bitmap]::FromFile($sourcePath)
try {
    foreach ($entry in $sizes.GetEnumerator()) {
        $directory = Join-Path $resRoot "mipmap-$($entry.Key)"
        [System.IO.Directory]::CreateDirectory($directory) | Out-Null
        Save-Icon $sourceBitmap $entry.Value (Join-Path $directory 'ic_launcher.png') $false
        Save-Icon $sourceBitmap $entry.Value (Join-Path $directory 'ic_launcher_round.png') $true
    }

    $foregroundDirectory = Join-Path $resRoot 'drawable-nodpi'
    [System.IO.Directory]::CreateDirectory($foregroundDirectory) | Out-Null
    $adaptive = New-CanvasBitmap 432 $true
    $adaptiveGraphics = [System.Drawing.Graphics]::FromImage($adaptive)
    try {
        Draw-Foreground $adaptiveGraphics $sourceBitmap 432 0.66
        $adaptive.Save((Join-Path $foregroundDirectory 'ic_launcher_foreground.png'), $pngFormat)
    } finally {
        $adaptiveGraphics.Dispose()
        $adaptive.Dispose()
    }
} finally {
    $sourceBitmap.Dispose()
}
