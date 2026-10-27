param(
    [string]$BoxImage = "C:\Users\Naiem Shaikh\.gemini\antigravity-ide\brain\8c7dd2c5-5b7d-4227-a059-337c919afa8c\.user_uploaded\media_1793085855391.png",
    [string]$WordmarkImage = "C:\Users\Naiem Shaikh\.gemini\antigravity-ide\brain\8c7dd2c5-5b7d-4227-a059-337c919afa8c\.user_uploaded\media_1793084235928.png",
    [string]$InstallerBoxIco = "d:\Website\ProducerToy-Plugins\ProVerb\Installer\installer_box.ico",
    [string]$InstalledProverbIco = "d:\Website\ProducerToy-Plugins\ProVerb\Installer\proverb.ico",
    [string]$OutputPng = "d:\Website\ProducerToy-Plugins\ProVerb\Resources\proverb_logo.png"
)

Add-Type -AssemblyName System.Drawing

Write-Host "======================================================================"
Write-Host "     BUILDING DUAL ICONS: 3D BOX (INSTALLER) & WORDMARK (POST-INSTALL)"
Write-Host "======================================================================"

# Progressive sharp downsampling function
function Downsample-Sharp($sourceImg, $tW, $tH) {
    $cur = $sourceImg
    $w = $cur.Width
    $h = $cur.Height
    while ($w -gt ($tW * 1.8) -and $h -gt ($tH * 1.8)) {
        $nW = [int]($w / 1.6)
        $nH = [int]($h / 1.6)
        $step = New-Object System.Drawing.Bitmap($nW, $nH, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $gs = [System.Drawing.Graphics]::FromImage($step)
        $gs.Clear([System.Drawing.Color]::Transparent)
        $gs.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $gs.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
        $gs.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::Half
        $gs.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
        $gs.DrawImage($cur, 0, 0, $nW, $nH)
        $gs.Dispose()
        if ($cur -ne $sourceImg) { $cur.Dispose() }
        $cur = $step
        $w = $nW
        $h = $nH
    }
    $final = New-Object System.Drawing.Bitmap($tW, $tH, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $gf = [System.Drawing.Graphics]::FromImage($final)
    $gf.Clear([System.Drawing.Color]::Transparent)
    $gf.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $gf.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
    $gf.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::Half
    $gf.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
    $gf.DrawImage($cur, 0, 0, $tW, $tH)
    $gf.Dispose()
    if ($cur -ne $sourceImg) { $cur.Dispose() }
    return $final
}

# Function to save multi-resolution Windows ICO (256 PNG, 128..16 32-bit ARGB DIB)
function Save-MultiResIco($masterBitmap, $outputIcoPath, $fitMode) {
    $sizes = @(256, 128, 64, 48, 32, 16)
    $aspect = [double]$masterBitmap.Width / [double]$masterBitmap.Height
    $entries = @()

    foreach ($size in $sizes) {
        if ($fitMode -eq "box") {
            if ($aspect -gt 1.0) {
                $destW = [int]($size * 0.98)
                $destH = [Math]::Max(1, [int]($destW / $aspect))
            } else {
                $destH = [int]($size * 0.98)
                $destW = [Math]::Max(1, [int]($destH * $aspect))
            }
        } else {
            # Wordmark mode: full width, proportional height
            $destW = $size
            $destH = [Math]::Max(1, [int]($destW / $aspect))
            if ($destH -gt $size) {
                $destH = $size
                $destW = [int]($destH * $aspect)
            }
        }

        $destX = [int](($size - $destW) / 2)
        $destY = [int](($size - $destH) / 2)

        $scaled = Downsample-Sharp $masterBitmap $destW $destH

        $canvas = New-Object System.Drawing.Bitmap($size, $size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $g = [System.Drawing.Graphics]::FromImage($canvas)
        $g.Clear([System.Drawing.Color]::Transparent)
        $g.DrawImage($scaled, $destX, $destY)
        $g.Dispose()
        $scaled.Dispose()

        if ($size -ge 256) {
            $ms = New-Object System.IO.MemoryStream
            $canvas.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
            $bytes = $ms.ToArray()
            $ms.Dispose()

            $entries += [PSCustomObject]@{
                Size     = $size
                Width    = 0
                Height   = 0
                BitCount = 32
                Data     = $bytes
            }
        } else {
            $pixelBytes = $size * $size * 4
            $andMaskRowBytes = [int]([Math]::Ceiling($size / 32.0) * 4)
            $andMaskBytes = $andMaskRowBytes * $size

            $dibStream = New-Object System.IO.MemoryStream
            $dibWriter = New-Object System.IO.BinaryWriter($dibStream)

            $dibWriter.Write([uint32]40)
            $dibWriter.Write([int32]$size)
            $dibWriter.Write([int32]($size * 2))
            $dibWriter.Write([uint16]1)
            $dibWriter.Write([uint16]32)
            $dibWriter.Write([uint32]0)
            $dibWriter.Write([uint32]($pixelBytes + $andMaskBytes))
            $dibWriter.Write([int32]0)
            $dibWriter.Write([int32]0)
            $dibWriter.Write([uint32]0)
            $dibWriter.Write([uint32]0)

            for ($y = $size - 1; $y -ge 0; $y--) {
                for ($x = 0; $x -lt $size; $x++) {
                    $p = $canvas.GetPixel($x, $y)
                    $dibWriter.Write([byte]$p.B)
                    $dibWriter.Write([byte]$p.G)
                    $dibWriter.Write([byte]$p.R)
                    $dibWriter.Write([byte]$p.A)
                }
            }

            $zeroMask = New-Object byte[] $andMaskBytes
            $dibWriter.Write($zeroMask)

            $dibWriter.Flush()
            $bytes = $dibStream.ToArray()
            $dibWriter.Close()
            $dibStream.Close()

            $entries += [PSCustomObject]@{
                Size     = $size
                Width    = [byte]$size
                Height   = [byte]$size
                BitCount = 32
                Data     = $bytes
            }
        }
        $canvas.Dispose()
    }

    $fs = [System.IO.File]::Create($outputIcoPath)
    $w = New-Object System.IO.BinaryWriter($fs)

    $w.Write([uint16]0)
    $w.Write([uint16]1)
    $w.Write([uint16]$entries.Count)

    $offset = 6 + ($entries.Count * 16)

    foreach ($entry in $entries) {
        $w.Write([byte]$entry.Width)
        $w.Write([byte]$entry.Height)
        $w.Write([byte]0)
        $w.Write([byte]0)
        $w.Write([uint16]1)
        $w.Write([uint16]32)
        $w.Write([uint32]$entry.Data.Length)
        $w.Write([uint32]$offset)
        $offset += $entry.Data.Length
    }

    foreach ($entry in $entries) {
        $w.Write($entry.Data)
    }

    $w.Flush()
    $w.Close()
    $fs.Close()
    Write-Host "[OK] Built ICO at: $outputIcoPath"
}

# -------------------------------------------------------------
# 1. Wordmark Processing (for After-Installation & Wizard Header)
# -------------------------------------------------------------
Copy-Item -Force $WordmarkImage $OutputPng
Write-Host "[OK] Copied high-res wordmark to Resources: $OutputPng"

$wmSrc = [System.Drawing.Bitmap]::FromFile($WordmarkImage)
$minX = $wmSrc.Width; $maxX = 0; $minY = $wmSrc.Height; $maxY = 0
for ($y = 0; $y -lt $wmSrc.Height; $y += 2) {
    for ($x = 0; $x -lt $wmSrc.Width; $x += 2) {
        if ($wmSrc.GetPixel($x, $y).A -gt 15) {
            if ($x -lt $minX) { $minX = $x }
            if ($x -gt $maxX) { $maxX = $x }
            if ($y -lt $minY) { $minY = $y }
            if ($y -gt $maxY) { $maxY = $y }
        }
    }
}
$wmW = $maxX - $minX + 1
$wmH = $maxY - $minY + 1
$wmAspect = [double]$wmW / [double]$wmH

$wmMaster = New-Object System.Drawing.Bitmap($wmW, $wmH, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$gWm = [System.Drawing.Graphics]::FromImage($wmMaster)
$gWm.Clear([System.Drawing.Color]::Transparent)
$gWm.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$gWm.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
$gWm.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
$gWm.DrawImage($wmSrc, 0, 0, (New-Object System.Drawing.Rectangle($minX, $minY, $wmW, $wmH)), [System.Drawing.GraphicsUnit]::Pixel)
$gWm.Dispose()
$wmSrc.Dispose()

# Save Installed App / Post-Install Icon: proverb.ico
Save-MultiResIco $wmMaster $InstalledProverbIco "wordmark"

# Wizard Small Headers (55x55 and 110x110)
$wizardSmallPng = "d:\Website\ProducerToy-Plugins\ProVerb\Installer\wizard_small.png"
$wizardSmallBmp = "d:\Website\ProducerToy-Plugins\ProVerb\Installer\wizard_small.bmp"
$smallCanvas = New-Object System.Drawing.Bitmap(55, 55, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$gSmall = [System.Drawing.Graphics]::FromImage($smallCanvas)
$gSmall.Clear([System.Drawing.Color]::Transparent)
$gSmall.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$gSmall.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
$gSmall.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality

$sW = 54
$sH = [int]($sW / $wmAspect)
$sX = [int]((55 - $sW) / 2)
$sY = [int]((55 - $sH) / 2)
$gSmall.DrawImage($wmMaster, $sX, $sY, $sW, $sH)
$gSmall.Dispose()
$smallCanvas.Save($wizardSmallPng, [System.Drawing.Imaging.ImageFormat]::Png)
$smallCanvas.Save($wizardSmallBmp, [System.Drawing.Imaging.ImageFormat]::Bmp)
$smallCanvas.Dispose()

$wizardSmall2xPng = "d:\Website\ProducerToy-Plugins\ProVerb\Installer\wizard_small_2x.png"
$wizardSmall2xBmp = "d:\Website\ProducerToy-Plugins\ProVerb\Installer\wizard_small_2x.bmp"
$small2xCanvas = New-Object System.Drawing.Bitmap(110, 110, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$gSmall2x = [System.Drawing.Graphics]::FromImage($small2xCanvas)
$gSmall2x.Clear([System.Drawing.Color]::Transparent)
$gSmall2x.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$gSmall2x.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
$gSmall2x.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality

$sW2 = 108
$sH2 = [int]($sW2 / $wmAspect)
$sX2 = [int]((110 - $sW2) / 2)
$sY2 = [int]((110 - $sH2) / 2)
$gSmall2x.DrawImage($wmMaster, $sX2, $sY2, $sW2, $sH2)
$gSmall2x.Dispose()
$small2xCanvas.Save($wizardSmall2xPng, [System.Drawing.Imaging.ImageFormat]::Png)
$small2xCanvas.Save($wizardSmall2xBmp, [System.Drawing.Imaging.ImageFormat]::Bmp)
$small2xCanvas.Dispose()
$wmMaster.Dispose()
Write-Host "[OK] Built transparent wizard small headers (55x55 and 110x110 PNG & BMP)"

# -------------------------------------------------------------
# 2. 3D Product Box Processing (for Installer EXE Only)
# -------------------------------------------------------------
$boxSrc = [System.Drawing.Bitmap]::FromFile($BoxImage)
$bMinX = $boxSrc.Width; $bMaxX = 0; $bMinY = $boxSrc.Height; $bMaxY = 0
for ($y = 0; $y -lt $boxSrc.Height; $y += 2) {
    for ($x = 0; $x -lt $boxSrc.Width; $x += 2) {
        if ($boxSrc.GetPixel($x, $y).A -gt 15) {
            if ($x -lt $bMinX) { $bMinX = $x }
            if ($x -gt $bMaxX) { $bMaxX = $x }
            if ($y -lt $bMinY) { $bMinY = $y }
            if ($y -gt $bMaxY) { $bMaxY = $y }
        }
    }
}
$bW = $bMaxX - $bMinX + 1
$bH = $bMaxY - $bMinY + 1

$boxMaster = New-Object System.Drawing.Bitmap($bW, $bH, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$gBm = [System.Drawing.Graphics]::FromImage($boxMaster)
$gBm.Clear([System.Drawing.Color]::Transparent)
$gBm.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$gBm.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
$gBm.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
$gBm.DrawImage($boxSrc, 0, 0, (New-Object System.Drawing.Rectangle($bMinX, $bMinY, $bW, $bH)), [System.Drawing.GraphicsUnit]::Pixel)
$gBm.Dispose()
$boxSrc.Dispose()

# Save Installer EXE Icon: installer_box.ico
Save-MultiResIco $boxMaster $InstallerBoxIco "box"
$boxMaster.Dispose()

Write-Host "======================================================================"
Write-Host "     ALL ASSETS BUILT SUCCESSFULLY!"
Write-Host "     Installer Icon (3D Box)   : $InstallerBoxIco"
Write-Host "     Post-Install Icon (Wordmark): $InstalledProverbIco"
Write-Host "======================================================================"
