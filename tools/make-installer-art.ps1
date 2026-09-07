# Renders the two bitmaps the NSIS wizard shows, from the same mark the
# application uses.
#
# BMP rather than PNG because MUI only accepts BMP, and 24-bit rather than 32
# because the installer's own compositing mishandles an alpha channel - a
# transparent header renders as a black block on some themes.

Add-Type -AssemblyName System.Drawing

$root = "C:\Users\victus\OneDrive\Desktop\claude works\keys"
$out  = Join-Path $root "resources\icons\generated"

# The graphite the application actually paints, sampled from a running window
# rather than converted from the OKLCH source - so the installer and the editor
# agree to the pixel.
$chrome  = [System.Drawing.Color]::FromArgb(14, 17, 20)
$surface = [System.Drawing.Color]::FromArgb(18, 21, 25)
$accent  = [System.Drawing.Color]::FromArgb(61, 126, 255)
$text    = [System.Drawing.Color]::FromArgb(232, 234, 237)
$muted   = [System.Drawing.Color]::FromArgb(138, 145, 155)

function Save-Bmp {
    param([System.Drawing.Bitmap]$Bitmap, [string]$Path)
    # Flatten to 24bpp: MUI draws the header over a dialog background and an
    # alpha channel comes out black.
    $flat = New-Object System.Drawing.Bitmap($Bitmap.Width, $Bitmap.Height,
                                             [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
    $g = [System.Drawing.Graphics]::FromImage($flat)
    $g.DrawImage($Bitmap, 0, 0, $Bitmap.Width, $Bitmap.Height)
    $g.Dispose()
    $flat.Save($Path, [System.Drawing.Imaging.ImageFormat]::Bmp)
    $flat.Dispose()
}

# ---- Header: 150x57, sits top-right on every page after the welcome --------
$header = New-Object System.Drawing.Bitmap(150, 57)
$g = [System.Drawing.Graphics]::FromImage($header)
$g.SmoothingMode = 'AntiAlias'
$g.InterpolationMode = 'HighQualityBicubic'
$g.TextRenderingHint = 'ClearTypeGridFit'
$g.Clear($chrome)

$icon = [System.Drawing.Image]::FromFile((Join-Path $out "keys-064.png"))
$g.DrawImage($icon, 14, 12, 33, 33)
$icon.Dispose()

$font = New-Object System.Drawing.Font("Segoe UI", 15, [System.Drawing.FontStyle]::Regular,
                                       [System.Drawing.GraphicsUnit]::Pixel)
$brush = New-Object System.Drawing.SolidBrush($text)
$g.DrawString("Keys", $font, $brush, 55, 20)
$g.Dispose()

Save-Bmp -Bitmap $header -Path (Join-Path $out "installer-header.bmp")
$header.Dispose()

# ---- Wizard sidebar: 164x314, the welcome and finish pages -----------------
$side = New-Object System.Drawing.Bitmap(164, 314)
$g = [System.Drawing.Graphics]::FromImage($side)
$g.SmoothingMode = 'AntiAlias'
$g.InterpolationMode = 'HighQualityBicubic'
$g.TextRenderingHint = 'ClearTypeGridFit'

# A flat panel, not a gradient. The design forbids gradients, and one here
# would be the first thing a user sees.
$g.Clear($chrome)

# A single accent rule down the right edge, the same device the active tab and
# the activity rail use.
$accentBrush = New-Object System.Drawing.SolidBrush($accent)
$g.FillRectangle($accentBrush, 161, 0, 3, 314)

$icon = [System.Drawing.Image]::FromFile((Join-Path $out "keys-128.png"))
$g.DrawImage($icon, 40, 74, 84, 84)
$icon.Dispose()

$nameFont = New-Object System.Drawing.Font("Segoe UI", 26, [System.Drawing.FontStyle]::Regular,
                                           [System.Drawing.GraphicsUnit]::Pixel)
$nameBrush = New-Object System.Drawing.SolidBrush($text)
$fmt = New-Object System.Drawing.StringFormat
$fmt.Alignment = 'Center'
$g.DrawString("Keys", $nameFont, $nameBrush,
              (New-Object System.Drawing.RectangleF(0, 176, 161, 40)), $fmt)

$subFont = New-Object System.Drawing.Font("Segoe UI", 12, [System.Drawing.FontStyle]::Regular,
                                          [System.Drawing.GraphicsUnit]::Pixel)
$subBrush = New-Object System.Drawing.SolidBrush($muted)
$g.DrawString("A focused editor", $subFont, $subBrush,
              (New-Object System.Drawing.RectangleF(0, 212, 161, 20)), $fmt)
$g.Dispose()

Save-Bmp -Bitmap $side -Path (Join-Path $out "installer-side.bmp")
$side.Dispose()

foreach ($f in @("installer-header.bmp", "installer-side.bmp")) {
    $p = Join-Path $out $f
    $i = [System.Drawing.Image]::FromFile($p)
    "  {0,-24} {1}x{2}  {3}" -f $f, $i.Width, $i.Height, $i.PixelFormat
    $i.Dispose()
}
