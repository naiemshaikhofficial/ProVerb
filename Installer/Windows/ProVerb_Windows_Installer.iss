; ==============================================================
; Producer Toy ProVerb Official Windows Audio Plugin Installer
; Supported Formats: VST3 (64-bit), AAX (Pro Tools 64-bit), VST2 (Legacy 64-bit)
; ==============================================================

[Setup]
AppId=ProVerb
AppName=ProVerb
AppVerName=ProVerb
AppVersion=1.0.0
AppPublisher=Producer Toy
AppPublisherURL=https://producertoy.com
AppSupportURL=https://producertoy.com
DefaultDirName={commoncf64}\VST3\ProVerb.vst3
DisableDirPage=yes
DefaultGroupName=Producer Toy
DisableProgramGroupPage=yes
OutputBaseFilename=ProVerb
OutputDir=..\..\Output
Compression=lzma2/ultra64
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
WizardImageFile=..\wizard_large.bmp,..\wizard_large_2x.bmp
WizardSmallImageFile=..\wizard_small.png,..\wizard_small_2x.png
SetupIconFile=..\proverb.ico
UninstallDisplayName=ProVerb
UninstallDisplayIcon={commoncf64}\ProducerToy\ProVerb\proverb.ico
UninstallFilesDir={commoncf64}\ProducerToy\ProVerb
DisableReadyPage=no
VersionInfoVersion=1.0.0.0
VersionInfoCompany=Producer Toy
VersionInfoDescription=ProVerb
VersionInfoTextVersion=1.0.0
VersionInfoProductName=ProVerb
VersionInfoProductVersion=1.0.0.0
VersionInfoCopyright=Copyright (C) 2026 Producer Toy

[Types]
Name: "full"; Description: "Standard Installation (Universal VST3 - All DAWs Recommended)"
Name: "custom"; Description: "Custom Plugin Formats"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 (Universal 32-bit & 64-bit) — (FL Studio, Ableton Live, Cubase, Studio One, Reaper, Bitwig)"; Types: full custom; Flags: fixed
Name: "aax"; Description: "AAX (64-bit) — Avid Pro Tools 11+"; Types: custom
Name: "vst2"; Description: "VST2 (Legacy) — Older DAWs / Custom VST Folder"; Types: custom; ExtraDiskSpaceRequired: 13107200

[Messages]
FinishedHeadingLabel=ProVerb Installed
FinishedLabel=ProVerb has been successfully installed on your computer.%n%nUniversal 32-bit and 64-bit compatibility ready for all DAWs:%n• FL Studio: Effects -> Reverb -> ProVerb%n• Ableton Live: Plug-ins -> VST3 -> Producer Toy%n• Cubase / Nuendo: Reverb -> ProVerb%n• Studio One: Effects -> Producer Toy -> ProVerb%n• Reaper: VST3: ProVerb (Producer Toy)%n• Bitwig Studio: Devices -> Reverb -> ProVerb%n• Pro Tools: Avid Audio Plug-Ins (AAX)%n%nOpen your DAW to activate ProVerb with your license.%nClick Finish to complete setup.
ConfirmUninstall=Are you sure you want to completely remove ProVerb and all of its components from your computer?

[InstallDelete]
; Safely clean up legacy duplicate installation in vendor subfolder from earlier versions
Type: filesandordirs; Name: "{commoncf64}\VST3\Producer Toy\ProVerb.vst3"
Type: filesandordirs; Name: "{commoncf32}\VST3\Producer Toy\ProVerb.vst3"; Check: IsWin64

[UninstallDelete]
Type: files; Name: "{commoncf64}\ProducerToy\ProVerb\ProducerToy_ProVerb_Uninstall.exe"
Type: files; Name: "{commoncf64}\ProducerToy\ProVerb\ProducerToy_ProVerb_Uninstall.dat"
Type: files; Name: "{commoncf64}\ProducerToy\ProVerb\proverb.ico"
Type: files; Name: "{commoncf64}\ProducerToy\ProVerb\unins000.exe"
Type: files; Name: "{commoncf64}\ProducerToy\ProVerb\unins000.dat"
Type: dirifempty; Name: "{commoncf64}\ProducerToy\ProVerb"

[Files]
; 0. Official Producer Toy ProVerb Logo for Windows Installed Apps / Control Panel
Source: "..\proverb.ico"; DestDir: "{commoncf64}\ProducerToy\ProVerb"; Flags: ignoreversion uninsneveruninstall

; 1a. Canonical Universal VST3 Plugin Bundle for 64-bit DAWs (contains both x86_64 and x86 binaries)
Source: "..\..\build\ProVerb_artefacts\Release\VST3\ProVerb.vst3\*"; DestDir: "{commoncf64}\VST3\ProVerb.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: vst3; Excludes: "*.pdb,*.ilk,*.exp,*.lib,*.obj,*.cpp,*.h,*.py,*.npz,*.git*,*.md,*.txt"

; 1b. Canonical Universal VST3 Plugin Bundle for 32-bit DAWs on 64-bit Windows
Source: "..\..\build\ProVerb_artefacts\Release\VST3\ProVerb.vst3\*"; DestDir: "{commoncf32}\VST3\ProVerb.vst3"; Check: IsWin64; Flags: ignoreversion recursesubdirs createallsubdirs; Components: vst3; Excludes: "*.pdb,*.ilk,*.exp,*.lib,*.obj,*.cpp,*.h,*.py,*.npz,*.git*,*.md,*.txt"

; 2. Official AAX Plugin Bundle for Pro Tools (if built)
Source: "..\..\build\ProVerb_artefacts\Release\AAX\ProVerb.aaxplugin\*"; DestDir: "{commoncf}\Avid\Audio\Plug-Ins\ProVerb.aaxplugin"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist; Components: aax

; 3. VST2 Legacy Plugin (if built)
Source: "..\..\build\ProVerb_artefacts\Release\VST\ProVerb.dll"; DestDir: "{code:GetVST2Dir}"; Flags: ignoreversion skipifsourcedoesntexist; Components: vst2

[Dirs]
Name: "{userappdata}\ProducerToy\Licenses"; Flags: uninsneveruninstall
Name: "{userdocs}\ProducerToy\ProVerb\Presets"; Flags: uninsneveruninstall

[Code]
var
  VST2DirPage: TInputDirWizardPage;

procedure InitializeWizard;
var
  DefaultVST2Path: String;
begin
  // Query standard registry for user's configured 64-bit VST2 path
  if not RegQueryStringValue(HKEY_LOCAL_MACHINE, 'SOFTWARE\VST', 'VSTPluginsPath', DefaultVST2Path) then
  begin
    if DirExists(ExpandConstant('{pf}\VstPlugins')) then
      DefaultVST2Path := ExpandConstant('{pf}\VstPlugins')
    else if DirExists(ExpandConstant('{pf}\Steinberg\VstPlugins')) then
      DefaultVST2Path := ExpandConstant('{pf}\Steinberg\VstPlugins')
    else
      DefaultVST2Path := ExpandConstant('{commoncf}\VST2');
  end;

  // Custom page for VST2 folder selection (only shown if VST2 checkbox is ticked)
  VST2DirPage := CreateInputDirPage(
    wpSelectComponents,
    'Select VST2 64-bit Plugin Location',
    'Where is your DAW''s 64-bit VST2 plugin folder located?',
    'Select the folder where your DAW scans for 64-bit VST plugins, then click Next.',
    False, '');
  VST2DirPage.Add('');
  VST2DirPage.Values[0] := DefaultVST2Path;
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  // Only show the VST2 folder selection page if user ticked the VST2 component checkbox
  if (PageID = VST2DirPage.ID) then
    Result := not WizardIsComponentSelected('vst2')
  else
    Result := False;
end;

function GetVST2Dir(Param: String): String;
begin
  Result := VST2DirPage.Values[0];
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  UninstExe, NewUninstExe: String;
  UninstDat, NewUninstDat: String;
  RegKey: String;
begin
  if CurStep = ssPostInstall then
  begin
    UninstExe := ExpandConstant('{commoncf64}\ProducerToy\ProVerb\unins000.exe');
    NewUninstExe := ExpandConstant('{commoncf64}\ProducerToy\ProVerb\ProducerToy_ProVerb_Uninstall.exe');
    UninstDat := ExpandConstant('{commoncf64}\ProducerToy\ProVerb\unins000.dat');
    NewUninstDat := ExpandConstant('{commoncf64}\ProducerToy\ProVerb\ProducerToy_ProVerb_Uninstall.dat');

    // Clean up any existing renamed files from prior installations
    if FileExists(NewUninstExe) and (UninstExe <> NewUninstExe) then
      DeleteFile(NewUninstExe);
    if FileExists(NewUninstDat) and (UninstDat <> NewUninstDat) then
      DeleteFile(NewUninstDat);

    // Rename unins000.* to ProducerToy_ProVerb_Uninstall.*
    if FileExists(UninstExe) then
      RenameFile(UninstExe, NewUninstExe);
    if FileExists(UninstDat) then
      RenameFile(UninstDat, NewUninstDat);

    // Update Windows Uninstall Registry entry with branded uninstaller executable path
    RegKey := 'Software\Microsoft\Windows\CurrentVersion\Uninstall\ProVerb_is1';
    RegWriteStringValue(HKEY_LOCAL_MACHINE, RegKey, 'UninstallString', '"' + NewUninstExe + '"');
    RegWriteStringValue(HKEY_LOCAL_MACHINE, RegKey, 'QuietUninstallString', '"' + NewUninstExe + '" /VERYSILENT');
    RegWriteStringValue(HKEY_LOCAL_MACHINE, RegKey, 'DisplayIcon', ExpandConstant('{commoncf64}\ProducerToy\ProVerb\proverb.ico'));
    RegWriteStringValue(HKEY_LOCAL_MACHINE, RegKey, 'DisplayName', 'ProVerb');

    // Clean up legacy registry key from earlier versions if present
    RegDeleteKeyIncludingSubkeys(HKEY_LOCAL_MACHINE, 'Software\Microsoft\Windows\CurrentVersion\Uninstall\Producer Toy ProVerb_is1');
  end;
end;
