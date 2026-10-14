Add-Type -AssemblyName System.Drawing

$srcPath = "D:\Website\ProducerToy-Plugins\ProVerb\Resources\wizard_large_2x.png"
$dst2xPath = "D:\Website\ProducerToy-Plugins\ProVerb\Installer\wizard_large_2x.bmp"
$dst1xPath = "D:\Website\ProducerToy-Plugins\ProVerb\Installer\wizard_large.bmp"

# 1. Load source PNG
$srcImg = [System.Drawing.Image]::FromFile($srcPath)
Write-Host "Source image size: $($srcImg.Width) x $($srcImg.Height)"

# 2. Save 2x BMP (328 x 628)
# Create a 24/32-bit RGB bitmap
$bmp2x = New-Object System.Drawing.Bitmap(328, 628, [System.Drawing.Imaging.PixelFormat]::Format32bppRgb)
$g2x = [System.Drawing.Graphics]::FromImage($bmp2x)
$g2x.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g2x.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
$g2x.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
$g2x.Clear([System.Drawing.Color]::Black)
$g2x.DrawImage($srcImg, 0, 0, 328, 628)
$g2x.Dispose()

$bmp2x.Save($dst2xPath, [System.Drawing.Imaging.ImageFormat]::Bmp)
$bmp2x.Dispose()
Write-Host "Saved 2x BMP to $dst2xPath"

# 3. Save 1x BMP (164 x 314)
$bmp1x = New-Object System.Drawing.Bitmap(164, 314, [System.Drawing.Imaging.PixelFormat]::Format32bppRgb)
$g1x = [System.Drawing.Graphics]::FromImage($bmp1x)
$g1x.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g1x.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
$g1x.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
$g1x.Clear([System.Drawing.Color]::Black)
$g1x.DrawImage($srcImg, 0, 0, 164, 314)
$g1x.Dispose()

$bmp1x.Save($dst1xPath, [System.Drawing.Imaging.ImageFormat]::Bmp)
$bmp1x.Dispose()
Write-Host "Saved 1x BMP to $dst1xPath"

$srcImg.Dispose()
Write-Host "Conversion completed successfully!"
