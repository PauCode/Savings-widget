$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.Drawing

$workspaceRoot = Split-Path -Parent $PSScriptRoot
$sourcePng = Join-Path $workspaceRoot 'Theme\default\icon.png'
$outputIco = Join-Path $workspaceRoot 'data\AppIcon.ico'

$sizes = @(16, 32, 48, 64, 128, 256)
$source = [System.Drawing.Bitmap]::FromFile($sourcePng)

$entries = @()
foreach ($size in $sizes) {
    $resized = New-Object System.Drawing.Bitmap $size, $size
    $graphics = [System.Drawing.Graphics]::FromImage($resized)
    $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
    $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $graphics.DrawImage($source, 0, 0, $size, $size)
    $graphics.Dispose()

    $stream = New-Object System.IO.MemoryStream
    $resized.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
    $resized.Dispose()

    $entries += , @{ Size = $size; Bytes = $stream.ToArray() }
    $stream.Dispose()
}
$source.Dispose()

$headerSize = 6
$dirEntrySize = 16
$dataOffset = $headerSize + ($dirEntrySize * $entries.Count)

$writer = New-Object System.IO.BinaryWriter([System.IO.File]::Create($outputIco))
try {
    $writer.Write([UInt16]0)      # reserved
    $writer.Write([UInt16]1)      # type: icon
    $writer.Write([UInt16]$entries.Count)

    $offset = $dataOffset
    foreach ($entry in $entries) {
        $byteSize = if ($entry.Size -ge 256) { 0 } else { $entry.Size }
        $writer.Write([Byte]$byteSize)   # width
        $writer.Write([Byte]$byteSize)   # height
        $writer.Write([Byte]0)           # color count
        $writer.Write([Byte]0)           # reserved
        $writer.Write([UInt16]1)         # color planes
        $writer.Write([UInt16]32)        # bits per pixel
        $writer.Write([UInt32]$entry.Bytes.Length)
        $writer.Write([UInt32]$offset)
        $offset += $entry.Bytes.Length
    }

    foreach ($entry in $entries) {
        $writer.Write($entry.Bytes)
    }
}
finally {
    $writer.Dispose()
}

Write-Output "Icon written to $outputIco"
