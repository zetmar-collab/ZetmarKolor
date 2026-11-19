; Instalator wtyczki Zetmar Kolor. Budowanie: package.ps1 (albo ISCC.exe /DLutDir=<folder z .cube> installer\ZetmarKolor.iss)
#define AppName "Zetmar Kolor / Zetmar Color"
#ifndef AppVer
  #define AppVer "1.0.0"
#endif
; Folder LUT-ow przygotowany przez package.ps1 (podfoldery = kategorie Biblioteki).
#ifndef LutDir
  #define LutDir "..\dist\zip\ZetmarKolor_" + AppVer + "\LUTs"
#endif

[Setup]
AppId={{B61642EA-A7CE-4990-B196-7CFBACE6A6C8}
AppName={#AppName}
AppVersion={#AppVer}
AppPublisher=Zetmar - Cyfrowy Przyjaciel
AppPublisherURL=https://zetmar.net.pl
DefaultDirName={commonappdata}\ZetmarKolor
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible
OutputDir=..\dist
OutputBaseFilename=ZetmarKolor_{#AppVer}_Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName={#AppName}

[Languages]
Name: "polish"; MessagesFile: "compiler:Languages\Polish.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[CustomMessages]
polish.PageTitle=Wybierz programy Adobe
polish.PageDesc=Wtyczka zostanie zainstalowana w zaznaczonych programach.
polish.ManualTitle=Folder wtyczek
polish.ManualDesc=Nie wykryto Photoshopa ani Photoshop Elements. Wskaż folder Plug-ins ręcznie.
polish.ManualHint=Wybierz folder Plug-ins (lub Plug-Ins) programu Photoshop / Photoshop Elements:
polish.NothingSelected=Zaznacz co najmniej jeden program.
polish.CopyFail=Nie udało się skopiować wtyczki do:
polish.Docs=Instrukcja instalacji i użycia
polish.LutFolder=Folder LUT-ów
english.PageTitle=Choose Adobe programs
english.PageDesc=The plug-in will be installed into the selected programs.
english.ManualTitle=Plug-ins folder
english.ManualDesc=Photoshop or Photoshop Elements was not detected. Choose the Plug-ins folder manually.
english.ManualHint=Choose the Plug-ins (or Plug-Ins) folder of Photoshop / Photoshop Elements:
english.NothingSelected=Select at least one program.
english.CopyFail=Could not copy the plug-in to:
english.Docs=Installation and usage guide
english.LutFolder=LUT folder
polish.DataDirUntrusted=Folder %1 już istnieje, ale jest dowiązaniem albo należy do konta innego niż administrator. Ze względów bezpieczeństwa instalacja została przerwana. Usuń lub zmień nazwę tego folderu i uruchom instalator ponownie.
english.DataDirUntrusted=The folder %1 already exists but is a link or belongs to a non-administrator account. For security reasons setup was stopped. Delete or rename that folder and run setup again.
polish.AclFailed=Nie udało się zabezpieczyć folderu danych:
english.AclFailed=Could not secure the data folder:
polish.AffinityItem=Affinity (folder wtyczek Zetmar Kolor)
polish.AffinityNote=Wtyczka została skopiowana do folderu:%n%1%n%nAffinity znajduje wtyczki w folderach Photoshopa i Elements automatycznie. Jeśli nie masz ich zainstalowanych, w Affinity otwórz Ustawienia > Photoshop Plugins, dodaj powyższy folder jako folder wyszukiwania i uruchom program ponownie.
english.AffinityItem=Affinity (Zetmar Color plug-in folder)
english.AffinityNote=The plug-in was copied to:%n%1%n%nAffinity finds plug-ins in Photoshop and Elements folders automatically. If you do not have them installed, open Affinity Settings > Photoshop Plugins, add the folder above as a search folder and restart the program.

[Dirs]
; Biblioteka LUT-ow: zwykly uzytkownik moze dodawac i usuwac pliki z poziomu wtyczki.
Name: "{app}\LUTs"; Permissions: users-modify

[Files]
Source: "..\build\ZetmarKolor_64.8bf"; DestDir: "{app}"; Flags: ignoreversion
Source: "INSTRUKCJA_PL.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "INSTALL_EN.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#LutDir}\*"; DestDir: "{app}\LUTs"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{commonprograms}\Zetmar Kolor\{cm:Docs} (PL)"; Filename: "{app}\INSTRUKCJA_PL.txt"
Name: "{commonprograms}\Zetmar Kolor\{cm:Docs} (EN)"; Filename: "{app}\INSTALL_EN.txt"
Name: "{commonprograms}\Zetmar Kolor\{cm:LutFolder}"; Filename: "{app}\LUTs"

[Code]
var
  ListPage: TWizardPage;
  Checks: TNewCheckListBox;
  ManualPage: TInputDirWizardPage;
  Paths: TStringList;
  Installed: string;
  AffinityDir: string;
  AffinityChosen: Boolean;

procedure AddPluginsDir(const Root, Caption: string);
var
  P: string;
begin
  P := Root + '\Plug-Ins';
  if not DirExists(P) then P := Root + '\Plug-ins';
  if DirExists(P) then
  begin
    Checks.AddCheckBox(Caption + '  (' + P + ')', '', 0, True, True, False, True, nil);
    Paths.Add(P);
  end;
end;

procedure ScanAdobe(const Base: string);
var
  FR: TFindRec;
  N: string;
begin
  if not FindFirst(Base + '\*', FR) then Exit;
  try
    repeat
      if (FR.Attributes and FILE_ATTRIBUTE_DIRECTORY <> 0) and (FR.Name <> '.') and (FR.Name <> '..') then
      begin
        N := FR.Name;
        if (Pos('Photoshop', N) > 0) and (Pos('Organizer', N) = 0) then
          AddPluginsDir(Base + '\' + N, N);
      end;
    until not FindNext(FR);
  finally
    FindClose(FR);
  end;
end;

{ Affinity (Photo / unified app): pakiet Sklepu (MSIX) albo klasyczna instalacja w Program Files\Affinity. }
function DetectAffinity: Boolean;
var
  RC: Integer;
begin
  Result := DirExists(ExpandConstant('{commonpf64}\Affinity'));
  if Result then Exit;
  if Exec(ExpandConstant('{sys}\WindowsPowerShell1.0\powershell.exe'),
    '-NoProfile -NonInteractive -Command "if (Get-AppxPackage -AllUsers | Where-Object { $_.Name -match ''Affinity'' }) { exit 0 } else { exit 1 }"',
    '', SW_HIDE, ewWaitUntilTerminated, RC) then
    Result := RC = 0;
end;

{ Folder danych w ProgramData: jesli juz istnieje, musi miec zaufanego wlasciciela (Administratorzy / SYSTEM /
  TrustedInstaller) i nie byc junction / linkiem. Inaczej zwykly uzytkownik moglby przekierowac instalacje. }
function InitializeSetup: Boolean;
var
  RC: Integer;
  Dir: string;
begin
  Result := True;
  Dir := ExpandConstant('{commonappdata}\ZetmarKolor');
  if not DirExists(Dir) then Exit;
  if Exec(ExpandConstant('{sys}\WindowsPowerShell\v1.0\powershell.exe'),
    '-NoProfile -NonInteractive -Command "$p = ''' + Dir + '''; $i = Get-Item -LiteralPath $p -Force; ' +
    'if ([int]$i.Attributes -band 1024) { exit 2 }; ' +
    '$o = (Get-Acl -LiteralPath $p).GetOwner([Security.Principal.SecurityIdentifier]).Value; ' +
    'if (@(''S-1-5-32-544'',''S-1-5-18'',''S-1-5-80-956008885-3418522649-1831038044-1853292631-2271478464'') -notcontains $o) { exit 3 }; exit 0"',
    '', SW_HIDE, ewWaitUntilTerminated, RC) then
  begin
    if RC <> 0 then
    begin
      SuppressibleMsgBox(FmtMessage(CustomMessage('DataDirUntrusted'), [Dir]), mbError, MB_OK, IDOK);
      Result := False;
    end;
  end;
end;

procedure InitializeWizard;
begin
  Paths := TStringList.Create;
  ListPage := CreateCustomPage(wpWelcome, CustomMessage('PageTitle'), CustomMessage('PageDesc'));
  Checks := TNewCheckListBox.Create(ListPage);
  Checks.Parent := ListPage.Surface;
  Checks.Left := 0;
  Checks.Top := 0;
  Checks.Width := ListPage.SurfaceWidth;
  Checks.Height := ListPage.SurfaceHeight;
  ScanAdobe(ExpandConstant('{commonpf64}\Adobe'));
  ScanAdobe(ExpandConstant('{commonpf32}\Adobe'));
  { Affinity nie ma stalego folderu wtyczek: osobny folder w ProgramData, dodawany w ustawieniach Affinity. }
  AffinityDir := ExpandConstant('{commonappdata}\ZetmarKolor\Plugins');
  if DetectAffinity then
  begin
    Checks.AddCheckBox(CustomMessage('AffinityItem') + '  (' + AffinityDir + ')', '', 0, True, True, False, True, nil);
    Paths.Add(AffinityDir);
  end;

  ManualPage := CreateInputDirPage(ListPage.ID, CustomMessage('ManualTitle'), CustomMessage('ManualDesc'),
    CustomMessage('ManualHint'), False, '');
  ManualPage.Add('');
  ManualPage.Values[0] := ExpandConstant('{commonpf64}\Adobe');
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := False;
  if PageID = ListPage.ID then Result := Paths.Count = 0;
  if PageID = ManualPage.ID then Result := Paths.Count > 0;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
var
  I, N: Integer;
begin
  Result := True;
  if CurPageID = ListPage.ID then
  begin
    N := 0;
    for I := 0 to Checks.Items.Count - 1 do
      if Checks.Checked[I] then N := N + 1;
    if N = 0 then
    begin
      MsgBox(CustomMessage('NothingSelected'), mbError, MB_OK);
      Result := False;
    end;
  end;
  if CurPageID = ManualPage.ID then
  begin
    if not DirExists(ManualPage.Values[0]) then
    begin
      MsgBox(CustomMessage('ManualHint'), mbError, MB_OK);
      Result := False;
    end;
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  I: Integer;
  Dst, Src: string;
  Targets: TStringList;
begin
  if CurStep = ssInstall then
  begin
    { Folder danych zamykamy PRZED kopiowaniem plikow: zapis tylko Administratorzy i SYSTEM, Users - odczyt.
      Prawo zapisu dla Users dostaje wylacznie podfolder LUTs (sekcja [Dirs]). }
    ForceDirectories(ExpandConstant('{app}'));
    if not Exec(ExpandConstant('{sys}\icacls.exe'),
      '"' + ExpandConstant('{app}') + '" /inheritance:r /grant:r *S-1-5-32-544:(OI)(CI)F *S-1-5-18:(OI)(CI)F *S-1-5-32-545:(OI)(CI)RX /Q',
      '', SW_HIDE, ewWaitUntilTerminated, I) or (I <> 0) then
      RaiseException(CustomMessage('AclFailed') + ' ' + ExpandConstant('{app}'));
  end;
  if CurStep <> ssPostInstall then Exit;
  Src := ExpandConstant('{app}\ZetmarKolor_64.8bf');
  Targets := TStringList.Create;
  try
    if Paths.Count > 0 then
    begin
      for I := 0 to Paths.Count - 1 do
        if Checks.Checked[I] then Targets.Add(Paths[I]);
    end
    else
      Targets.Add(ManualPage.Values[0]);
    for I := 0 to Targets.Count - 1 do
    begin
      if Targets[I] = AffinityDir then
      begin
        Dst := Targets[I];
        AffinityChosen := True;
      end
      else
        Dst := Targets[I] + '\Zetmar';
      ForceDirectories(Dst);
      if FileCopy(Src, Dst + '\ZetmarKolor_64.8bf', False) then
        Installed := Installed + Dst + ';'
      else
        MsgBox(CustomMessage('CopyFail') + #13#10 + Dst, mbError, MB_OK);
    end;
  finally
    Targets.Free;
  end;
  RegWriteStringValue(HKLM, 'Software\Zetmar\Kolor', 'Targets', Installed);
  if AffinityChosen then
    SuppressibleMsgBox(FmtMessage(CustomMessage('AffinityNote'), [AffinityDir]), mbInformation, MB_OK, IDOK);
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  S, D: string;
  P: Integer;
begin
  if CurUninstallStep <> usUninstall then Exit;
  if RegQueryStringValue(HKLM, 'Software\Zetmar\Kolor', 'Targets', S) then
  begin
    while S <> '' do
    begin
      P := Pos(';', S);
      if P = 0 then
      begin
        D := S;
        S := '';
      end
      else
      begin
        D := Copy(S, 1, P - 1);
        S := Copy(S, P + 1, MaxInt);
      end;
      if D <> '' then
      begin
        DeleteFile(D + '\ZetmarKolor_64.8bf');
        RemoveDir(D);  { tylko gdy pusty - folder Zetmar moze zawierac inne wtyczki }
      end;
    end;
    RegDeleteKeyIncludingSubkeys(HKLM, 'Software\Zetmar\Kolor');
  end;
end;
