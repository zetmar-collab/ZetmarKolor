# Instaluje wtyczke w Photoshop Elements / Photoshop oraz LUT-y do C:\ProgramData\ZetmarKolor\LUTs
# (wymaga uprawnien administratora - skrypt sam o nie poprosi).
# Uzycie: powershell -ExecutionPolicy Bypass -File install.ps1 [-Plugin sciezka\do\ZetmarKolor_64.8bf] [-Luts folder] [-Uninstall]
param([string]$Plugin = '', [string]$Luts = '', [switch]$Uninstall)

# Domyslnie: pliki obok skryptu (paczka ZIP), a w repozytorium build\ oraz ..\Photoshop\LUTS.
if (-not $Plugin) {
    $Plugin = Join-Path $PSScriptRoot 'ZetmarKolor_64.8bf'
    if (-not (Test-Path $Plugin)) { $Plugin = Join-Path $PSScriptRoot 'build\ZetmarKolor_64.8bf' }
}
if (-not $Luts) {
    # Repozytorium: LUTS\Zemar-...; paczka ZIP: LUTs obok skryptu (Windows nie rozroznia wielkosci liter).
    $Luts = Join-Path $PSScriptRoot 'LUTS\Zemar-Cyfrowy_Przyjaciel_60_LUTs'
    if (-not (Test-Path $Luts)) { $Luts = Join-Path $PSScriptRoot 'LUTs' }
}

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    $argList = @('-NoProfile','-ExecutionPolicy','Bypass','-File',"`"$PSCommandPath`"",'-Plugin',"`"$Plugin`"",'-Luts',"`"$Luts`"")
    if ($Uninstall) { $argList += '-Uninstall' }
    Start-Process powershell -Verb RunAs -ArgumentList $argList -Wait
    exit
}

$lutTarget = Join-Path $env:ProgramData 'ZetmarKolor\LUTs'
$targets = @()
foreach ($root in @("$env:ProgramFiles\Adobe", "${env:ProgramFiles(x86)}\Adobe")) {
    if (-not (Test-Path $root)) { continue }
    Get-ChildItem $root -Directory | Where-Object { $_.Name -match '^Adobe Photoshop \d|^Adobe Photoshop$|^Photoshop Elements \d{4}$' } | ForEach-Object {
        $pl = Join-Path $_.FullName 'Plug-Ins'
        if (-not (Test-Path $pl)) { $pl = Join-Path $_.FullName 'Plug-ins' }
        if (Test-Path $pl) { $targets += $pl }
    }
}

if ($Uninstall) {
    foreach ($t in $targets) {
        $f = Join-Path $t 'Zetmar\ZetmarKolor_64.8bf'
        if (Test-Path $f) { Remove-Item $f -Force; Write-Host "Usunieto / Removed: $f" }
        # Folder Zetmar moze zawierac inne wtyczki (np. Watermark) - usuwamy go tylko, gdy jest pusty.
        $dir = Join-Path $t 'Zetmar'
        if ((Test-Path $dir) -and -not (Get-ChildItem $dir)) { Remove-Item $dir -Force }
    }
    # LUT-y: usuwamy tylko pliki z paczki (te same sciezki wzgledne). Pliki dodane przez uzytkownika zostaja.
    if ((Test-Path $lutTarget) -and (Test-Path $Luts)) {
        $src = (Resolve-Path $Luts).Path
        Get-ChildItem $src -Recurse -File | ForEach-Object {
            $rel = $_.FullName.Substring($src.Length).TrimStart('\')
            $f = Join-Path $lutTarget $rel
            if (Test-Path $f) { Remove-Item $f -Force }
        }
        # Puste podfoldery (od najglebszych) i sam folder biblioteki - tylko jesli nic w nich nie zostalo.
        Get-ChildItem $lutTarget -Recurse -Directory | Sort-Object { $_.FullName.Length } -Descending | ForEach-Object {
            if (-not (Get-ChildItem $_.FullName -Force)) { Remove-Item $_.FullName -Force }
        }
        if (-not (Get-ChildItem $lutTarget -Force)) { Remove-Item $lutTarget -Force; Write-Host "Usunieto / Removed: $lutTarget" }
        else { Write-Host "Zachowano pliki uzytkownika w / Kept your own files in: $lutTarget" }
    } elseif (Test-Path $lutTarget) {
        Write-Host "Nie znaleziono paczki LUT-ow - biblioteka zostaje / LUT pack not found - library kept: $lutTarget"
    }
    Start-Sleep 2
    exit
}

if (-not (Test-Path $Plugin)) { Write-Host "Brak pliku / File not found: $Plugin"; Read-Host 'Enter'; exit 1 }
if (-not $targets) {
    Write-Host 'Nie znaleziono instalacji Photoshopa / Elements. / Photoshop / Elements installation not found.'
    Write-Host 'Zobacz INSTRUKCJA_PL.txt / See INSTALL_EN.txt (instalacja reczna / manual installation).'
    Read-Host 'Enter'
    exit 1
}
foreach ($t in $targets) {
    $dir = Join-Path $t 'Zetmar'
    New-Item -ItemType Directory $dir -Force | Out-Null
    Copy-Item $Plugin $dir -Force
    Write-Host "Zainstalowano / Installed: $dir"
}
if (Test-Path $Luts) {
    New-Item -ItemType Directory $lutTarget -Force | Out-Null
    # Cala zawartosc (podfoldery = kategorie Biblioteki).
    Copy-Item (Join-Path $Luts '*') $lutTarget -Recurse -Force
    # Biblioteka: zwykly uzytkownik (grupa Users, S-1-5-32-545) moze dodawac i usuwac pliki z poziomu wtyczki.
    & icacls $lutTarget /grant '*S-1-5-32-545:(OI)(CI)M' /T /Q | Out-Null
    Write-Host "LUT-y / LUTs: $lutTarget ($((Get-ChildItem $lutTarget -Filter *.cube -Recurse).Count))"
}
Start-Sleep 2
