# Buduje paczki dystrybucyjne do dist\:
#   ZetmarKolor_<wersja>.zip        - wtyczka, LUT-y, install.ps1, instrukcje PL/EN (instalacja reczna)
#   ZetmarKolor_<wersja>_Setup.exe  - instalator automatyczny (Inno Setup 6)
# LUT-y: folder z plikami .cube; jesli zawiera Catalog.csv (kolumny File, Category), pliki trafiaja
# do podfolderow kategorii (w Bibliotece wtyczki to kategorie). Kazdy plik jest sprawdzany parserem wtyczki.
# Uzycie: powershell -ExecutionPolicy Bypass -File package.ps1 [-LutDir folder] [-NoBuild]
param([string]$LutDir = "$PSScriptRoot\LUTS\Zemar-Cyfrowy_Przyjaciel_60_LUTs", [switch]$NoBuild)
$ErrorActionPreference = 'Stop'
$Version = (Get-Content (Join-Path $PSScriptRoot 'version.txt') -TotalCount 1).Trim()  # jedno zrodlo: version.txt
$root = $PSScriptRoot
$dist = Join-Path $root 'dist'

if (-not $NoBuild) {
    & cmd /c "`"$root\build.cmd`""
    if ($LASTEXITCODE -ne 0) { throw 'Budowanie nie powiodlo sie.' }
    & "$root\build\zk_tests.exe" | Select-Object -Last 1
    if ($LASTEXITCODE -ne 0) { throw 'Testy rdzenia nie przeszly.' }
}
$plugin = Join-Path $root 'build\ZetmarKolor_64.8bf'
if (-not (Test-Path $plugin)) { throw "Brak $plugin" }
$LutDir = (Resolve-Path $LutDir).Path
$luts = Get-ChildItem $LutDir -Filter *.cube
if (-not $luts) { throw "Brak plikow .cube w $LutDir" }
& "$root\build\zk_cubecheck.exe" $LutDir
if ($LASTEXITCODE -ne 0) { throw "Uszkodzone pliki .cube w $LutDir" }

# --- Katalog: plik -> kategoria
$category = @{}
$catalog = Join-Path $LutDir 'Catalog.csv'
if (Test-Path $catalog) {
    Import-Csv $catalog -Encoding UTF8 | ForEach-Object { if ($_.File -and $_.Category) { $category[$_.File] = $_.Category } }
}

# --- Przygotowanie zawartosci (wspolnej dla ZIP i instalatora)
$name = "ZetmarKolor_$Version"
$stage = Join-Path $dist "zip\$name"
if (Test-Path (Join-Path $dist 'zip')) { Remove-Item (Join-Path $dist 'zip') -Recurse -Force }
$lutStage = Join-Path $stage 'LUTs'
New-Item -ItemType Directory $lutStage -Force | Out-Null
Copy-Item $plugin, "$root\install.ps1", "$root\installer\INSTRUKCJA_PL.txt", "$root\installer\INSTALL_EN.txt" $stage
foreach ($f in $luts) {
    $dst = $lutStage
    if ($category.ContainsKey($f.Name)) { $dst = Join-Path $lutStage $category[$f.Name] }
    New-Item -ItemType Directory $dst -Force | Out-Null
    Copy-Item $f.FullName $dst
}
$readme = Join-Path $LutDir 'README.txt'
if (Test-Path $readme) { Copy-Item $readme (Join-Path $lutStage 'README_LUTs.txt') }

# --- ZIP
$zip = Join-Path $dist "$name.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path $stage -DestinationPath $zip -CompressionLevel Optimal
$cats = (Get-ChildItem $lutStage -Directory | ForEach-Object { "$($_.Name) ($((Get-ChildItem $_.FullName -Filter *.cube).Count))" }) -join ', '
Write-Host "ZIP: $zip ($($luts.Count) LUT: $cats)"

# --- Instalator
$iscc = @("$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe", "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
          "$env:ProgramFiles\Inno Setup 6\ISCC.exe") | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $iscc) { $iscc = (Get-Command ISCC.exe -ErrorAction SilentlyContinue).Source }
if (-not $iscc) { throw 'Nie znaleziono Inno Setup 6 (ISCC.exe).' }
& $iscc /Q "/DAppVer=$Version" "/DLutDir=$lutStage" "$root\installer\ZetmarKolor.iss"
if ($LASTEXITCODE -ne 0) { throw 'ISCC nie powiodl sie.' }
Write-Host "Instalator: $(Join-Path $dist "${name}_Setup.exe")"
