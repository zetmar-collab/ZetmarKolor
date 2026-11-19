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

# Skrypt dziala jako administrator na folderze, w ktorym zwykly uzytkownik moze tworzyc wpisy (ProgramData).
# Odmawiamy pracy, gdy folder (albo cos pod nim) jest junction / linkiem symbolicznym - inaczej zapis, zmiana
# uprawnien albo kasowanie moglyby trafic poza biblioteke.
function Assert-NoReparsePoint([string]$Path) {
    $p = $Path
    while ($p -and $p.Length -gt $env:ProgramData.Length) {
        if ((Test-Path -LiteralPath $p) -and ((Get-Item -LiteralPath $p -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw "Folder jest dowiazaniem (junction / link) / Folder is a reparse point: $p"
        }
        $p = Split-Path $p -Parent
    }
    if (Test-Path -LiteralPath $Path) {
        $bad = Get-ChildItem -LiteralPath $Path -Recurse -Force -Attributes ReparsePoint -ErrorAction SilentlyContinue
        if ($bad) { throw "Wewnatrz sa dowiazania (junction / link) / Reparse points inside: $($bad[0].FullName)" }
    }
}

# SID wlascicieli, ktorym ufamy w ProgramData: Administratorzy, SYSTEM, TrustedInstaller.
$trustedOwners = @('S-1-5-32-544', 'S-1-5-18', 'S-1-5-80-956008885-3418522649-1831038044-1853292631-2271478464')
function Assert-TrustedOwner([string]$Path) {
    $owner = (Get-Acl -LiteralPath $Path).GetOwner([Security.Principal.SecurityIdentifier]).Value
    if ($trustedOwners -notcontains $owner) {
        throw "Folder nalezy do innego uzytkownika (usun go albo zmien nazwe) / Folder is owned by a non-admin account (delete or rename it): $Path"
    }
}

# Folder ProgramData\ZetmarKolor: tworzony przez administratora i od razu zamykany (zapis tylko Administratorzy i
# SYSTEM, Users - odczyt). Dopiero potem biblioteka LUTs dostaje prawo zapisu dla Users. Gdy folder juz istnieje,
# musi miec zaufanego wlasciciela i nie moze byc linkiem - inaczej instalacja jest przerywana (nic nie kasujemy).
function Initialize-DataRoot {
    $root = Split-Path $lutTarget -Parent
    if (Test-Path -LiteralPath $root) {
        Assert-NoReparsePoint $root
        Assert-TrustedOwner $root
    } else {
        New-Item -ItemType Directory $root | Out-Null
    }
    & icacls $root /inheritance:r /grant:r '*S-1-5-32-544:(OI)(CI)F' '*S-1-5-18:(OI)(CI)F' '*S-1-5-32-545:(OI)(CI)RX' /Q | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "icacls nie powiodl sie / icacls failed: $root" }
    Assert-NoReparsePoint $root
}
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
        try { Assert-NoReparsePoint $lutTarget; Assert-TrustedOwner (Split-Path $lutTarget -Parent) } catch { Write-Host "Przerwano / Aborted: $($_.Exception.Message)"; Start-Sleep 4; exit 1 }
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
    try {
        Initialize-DataRoot
        Assert-NoReparsePoint $lutTarget
        if (-not (Test-Path -LiteralPath $lutTarget)) { New-Item -ItemType Directory $lutTarget | Out-Null }
        Assert-NoReparsePoint $lutTarget
        # Biblioteka: zwykly uzytkownik (Users, S-1-5-32-545) moze dodawac i usuwac pliki z poziomu wtyczki.
        # Prawo zapisu tylko na folderze LUTs (dziedziczone przez nowe pliki), bez /T i bez podazania za linkami.
        & icacls $lutTarget /grant '*S-1-5-32-545:(OI)(CI)M' /Q | Out-Null
        if ($LASTEXITCODE -ne 0) { throw "icacls nie powiodl sie / icacls failed: $lutTarget" }
        Assert-NoReparsePoint $lutTarget
    } catch { Write-Host "Przerwano / Aborted: $($_.Exception.Message)"; Read-Host 'Enter'; exit 1 }
    # Cala zawartosc (podfoldery = kategorie Biblioteki).
    Copy-Item (Join-Path $Luts '*') $lutTarget -Recurse -Force
    Write-Host "LUT-y / LUTs: $lutTarget ($((Get-ChildItem $lutTarget -Filter *.cube -Recurse).Count))"
}
Start-Sleep 2
