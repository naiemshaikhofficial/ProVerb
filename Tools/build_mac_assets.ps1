param(
    [string]$BoxImage = "C:\Users\Naiem Shaikh\.gemini\antigravity-ide\brain\8c7dd2c5-5b7d-4227-a059-337c919afa8c\.user_uploaded\media_1793085855391.png",
    [string]$WordmarkImage = "C:\Users\Naiem Shaikh\.gemini\antigravity-ide\brain\8c7dd2c5-5b7d-4227-a059-337c919afa8c\.user_uploaded\media_1793084235928.png",
    [string]$MacDir = "d:\Website\ProducerToy-Plugins\ProVerb\Installer\macOS"
)

Add-Type -AssemblyName System.Drawing

Write-Host "======================================================================"
Write-Host "         BUILDING OFFICIAL macOS ASSETS (ICNS, DMG & PKG)"
Write-Host "======================================================================"

if (-not (Test-Path $MacDir)) {
    New-Item -ItemType Directory -Path $MacDir -Force | Out-Null
}

function Build-AppleIcns($inputPngPath, $outputIcnsPath) {
    $src = [System.Drawing.Bitmap]::FromFile($inputPngPath)
    
    # Modern Apple ICNS standard sizes
    $targets = @(
        @{ Tag = "ic10"; Size = 1024 },
        @{ Tag = "ic09"; Size = 512 },
        @{ Tag = "ic14"; Size = 512 },
        @{ Tag = "ic08"; Size = 256 },
        @{ Tag = "ic13"; Size = 256 },
        @{ Tag = "ic07"; Size = 128 },
        @{ Tag = "ic12"; Size = 64 },
        @{ Tag = "ic11"; Size = 32 }
    )
    
    $aspect = [double]$src.Width / [double]$src.Height
    $chunks = @()
    
    foreach ($t in $targets) {
        $size = $t.Size
        $tag = $t.Tag
        
        $destW = $size
        $destH = [int]($destW / $aspect)
        if ($destH -gt $size) {
            $destH = $size
            $destW = [int]($destH * $aspect)
        }
        $destX = [int](($size - $destW) / 2)
        $destY = [int](($size - $destH) / 2)
        
        $canvas = New-Object System.Drawing.Bitmap($size, $size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $g = [System.Drawing.Graphics]::FromImage($canvas)
        $g.Clear([System.Drawing.Color]::Transparent)
        $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
        $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::Half
        $g.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
        $g.DrawImage($src, $destX, $destY, $destW, $destH)
        $g.Dispose()
        
        $ms = New-Object System.IO.MemoryStream
        $canvas.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
        $pngBytes = $ms.ToArray()
        $ms.Dispose()
        $canvas.Dispose()
        
        $chunks += @{
            Tag = $tag
            Data = $pngBytes
        }
    }
    $src.Dispose()
    
    $totalSize = 8
    foreach ($c in $chunks) {
        $totalSize += 8 + $c.Data.Length
    }
    
    $fs = [System.IO.File]::Create($outputIcnsPath)
    $bw = New-Object System.IO.BinaryWriter($fs)
    
    function Write-UInt32BE($w, [uint32]$val) {
        $b = [System.BitConverter]::GetBytes($val)
        if ([System.BitConverter]::IsLittleEndian) { [System.Array]::Reverse($b) }
        $w.Write($b)
    }
    
    # Header: 'icns' + total size
    $bw.Write([byte]0x69) # 'i'
    $bw.Write([byte]0x63) # 'c'
    $bw.Write([byte]0x6E) # 'n'
    $bw.Write([byte]0x73) # 's'
    Write-UInt32BE $bw $totalSize
    
    foreach ($c in $chunks) {
        $tagBytes = [System.Text.Encoding]::ASCII.GetBytes($c.Tag)
        $bw.Write($tagBytes)
        $chunkSize = [uint32](8 + $c.Data.Length)
        Write-UInt32BE $bw $chunkSize
        $bw.Write($c.Data)
    }
    
    $bw.Flush()
    $bw.Close()
    $fs.Close()
    Write-Host "[OK] Built Apple ICNS: $outputIcnsPath ($([Math]::Round($totalSize / 1024, 1)) KB)"
}

# 1. Build Apple ICNS Files
$boxIcns = Join-Path $MacDir "installer_box.icns"
$proverbIcns = Join-Path $MacDir "proverb.icns"
Build-AppleIcns $BoxImage $boxIcns
Build-AppleIcns $WordmarkImage $proverbIcns

# 2. Build Welcome Logo for Installer HTML
$welcomeLogoPath = Join-Path $MacDir "welcome_logo.png"
Copy-Item -Force $WordmarkImage $welcomeLogoPath
Write-Host "[OK] Built HTML Header Logo: $welcomeLogoPath"

# 3. Build DMG Background Image (800 x 500)
$dmgBgPath = Join-Path $MacDir "dmg_background.png"
$bgWidth = 800
$bgHeight = 500
$bgBmp = New-Object System.Drawing.Bitmap($bgWidth, $bgHeight, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$gBg = [System.Drawing.Graphics]::FromImage($bgBmp)
$gBg.Clear([System.Drawing.Color]::FromArgb(255, 18, 18, 20)) # Dark modern theme

# Subtle top gradient
$gradBrush = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
    (New-Object System.Drawing.Point(0, 0)),
    (New-Object System.Drawing.Point(0, 180)),
    [System.Drawing.Color]::FromArgb(255, 30, 30, 36),
    [System.Drawing.Color]::FromArgb(255, 18, 18, 20)
)
$gBg.FillRectangle($gradBrush, 0, 0, $bgWidth, 180)
$gradBrush.Dispose()

# Subtle outer border
$borderPen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(60, 255, 255, 255), 1)
$gBg.DrawRectangle($borderPen, 0, 0, $bgWidth - 1, $bgHeight - 1)
$borderPen.Dispose()

# Header Wordmark
$wmImg = [System.Drawing.Bitmap]::FromFile($WordmarkImage)
$wmW = 240
$wmH = [int]($wmW * ($wmImg.Height / $wmImg.Width))
$wmX = [int](($bgWidth - $wmW) / 2)
$wmY = 30
$gBg.DrawImage($wmImg, $wmX, $wmY, $wmW, $wmH)
$wmImg.Dispose()

# Subtitle Text
$font = New-Object System.Drawing.Font("Arial", 11, [System.Drawing.FontStyle]::Bold)
$textBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(200, 255, 255, 255))
$subText = "Official Universal Audio Plugin Installer for macOS"
$subSize = $gBg.MeasureString($subText, $font)
$gBg.DrawString($subText, $font, $textBrush, [int](($bgWidth - $subSize.Width) / 2), ($wmY + $wmH + 10))
$font.Dispose()
$textBrush.Dispose()

# Arrow pointing to install instructions
$guideFont = New-Object System.Drawing.Font("Arial", 10, [System.Drawing.FontStyle]::Regular)
$guideBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(150, 255, 255, 255))
$instructionText = "Double-click 'Install ProVerb.pkg' to install Audio Units (AU), VST3 & AAX"
$instrSize = $gBg.MeasureString($instructionText, $guideFont)
$gBg.DrawString($instructionText, $guideFont, $guideBrush, [int](($bgWidth - $instrSize.Width) / 2), ($bgHeight - 50))
$guideFont.Dispose()
$guideBrush.Dispose()

$gBg.Dispose()
$bgBmp.Save($dmgBgPath, [System.Drawing.Imaging.ImageFormat]::Png)
$bgBmp.Dispose()
Write-Host "[OK] Built Branded DMG Window Background: $dmgBgPath"

Write-Host "======================================================================"
Write-Host "            ALL macOS ASSETS GENERATED SUCCESSFULLY!"
Write-Host "======================================================================"
