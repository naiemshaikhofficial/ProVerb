; ==============================================================
; Producer Toy ProVerb Official Windows Audio Plugin Installer
; Supported Formats: VST3 (64-bit), AAX (Pro Tools 64-bit), VST2 (Legacy 64-bit)
; ==============================================================

[Setup]
AppName=Producer Toy ProVerb
AppVersion=1.0.0
AppPublisher=Producer Toy
AppPublisherURL=https://producertoy.com
AppSupportURL=https://producertoy.com
DefaultDirName={commoncf64}\VST3\ProVerb.vst3
DisableDirPage=yes
DefaultGroupName=Producer Toy
DisableProgramGroupPage=yes
OutputBaseFilename=ProducerToy_ProVerb_Windows_Installer
OutputDir=..\..\Output
Compression=lzma2/ultra64
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
WizardImageFile=..\wizard_large.bmp,..\wizard_large_2x.bmp
WizardSmallImageFile=..\wizard_small.bmp
SetupIconFile=..\proverb.ico
UninstallFilesDir={commoncf64}\ProducerToy\ProVerb
DisableReadyPage=no

[Types]
Name: "full"; Description: "Standard Installation (VST3 + Standalone App - Recommended)"
Name: "custom"; Description: "Custom Plugin Formats"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 (64-bit) — FL Studio, Ableton Live, Cubase, Studio One, Reaper, Bitwig"; Types: full custom
Name: "standalone"; Description: "Standalone Application (64-bit) — Live Performance & Testing"; Types: full custom
Name: "aax"; Description: "AAX (64-bit) — Avid Pro Tools 11+"; Types: full custom
Name: "vst2"; Description: "VST2 (64-bit Legacy) — Older DAWs / Custom VST Folder"; Types: custom

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut for Standalone ProVerb"; GroupDescription: "Additional shortcuts:"; Components: standalone

[Messages]
FinishedHeadingLabel=Producer Toy ProVerb Installed
FinishedLabel=Producer Toy ProVerb has been successfully installed on your computer.%n%nUniversal compatibility ready for all DAWs:%n• FL Studio: Effects -> Reverb -> ProVerb%n• Ableton Live: Plug-ins -> VST3 -> Producer Toy%n• Cubase / Nuendo: Reverb -> ProVerb%n• Studio One: Effects -> Producer Toy -> ProVerb%n• Reaper: VST3: ProVerb (Producer Toy)%n• Bitwig Studio: Devices -> Reverb -> ProVerb%n• Pro Tools: Avid Audio Plug-Ins (AAX)%n• Standalone App: Desktop & Start Menu%n%nOpen your DAW or launch ProVerb to activate with your ProducerToy.com account.%nClick Finish to complete setup.

[Files]
; 1a. Official Universal VST3 Plugin Bundle for DAWs (Root VST3 folder)
Source: "..\..\build\ProVerb_artefacts\Release\VST3\ProVerb.vst3\*"; DestDir: "{commoncf64}\VST3\ProVerb.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: vst3; Excludes: "*.pdb,*.ilk,*.exp,*.lib,*.obj,*.cpp,*.h,*.py,*.npz,*.git*,*.md,*.txt"

; 1b. Producer Toy Vendor VST3 Subfolder (For DAWs organizing by vendor: FL Studio, Cubase, Studio One)
Source: "..\..\build\ProVerb_artefacts\Release\VST3\ProVerb.vst3\*"; DestDir: "{commoncf64}\VST3\Producer Toy\ProVerb.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: vst3; Excludes: "*.pdb,*.ilk,*.exp,*.lib,*.obj,*.cpp,*.h,*.py,*.npz,*.git*,*.md,*.txt"

; 2. Standalone Executable Application & Runtime Dependencies
Source: "..\..\build\ProVerb_artefacts\Release\Standalone\*"; DestDir: "{autopf}\Producer Toy\ProVerb"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: standalone; Excludes: "*.pdb,*.ilk,*.exp,*.lib,*.obj,*.cpp,*.h,*.py,*.npz,*.git*,*.md,*.txt"

; 3. Official AAX Plugin Bundle for Pro Tools (if built)
Source: "..\..\build\ProVerb_artefacts\Release\AAX\ProVerb.aaxplugin\*"; DestDir: "{commoncf}\Avid\Audio\Plug-Ins\ProVerb.aaxplugin"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist; Components: aax

; 4. VST2 Legacy Plugin (if built)
Source: "..\..\build\ProVerb_artefacts\Release\VST\ProVerb.dll"; DestDir: "{code:GetVST2Dir}"; Flags: ignoreversion skipifsourcedoesntexist; Components: vst2

[Dirs]
Name: "{userappdata}\ProducerToy\Licenses"; Flags: uninsneveruninstall
Name: "{userdocs}\ProducerToy\ProVerb\Presets"; Flags: uninsneveruninstall

[Icons]
Name: "{autoprograms}\Producer Toy\ProVerb"; Filename: "{autopf}\Producer Toy\ProVerb\ProVerb.exe"; Components: standalone
Name: "{autodesktop}\ProVerb"; Filename: "{autopf}\Producer Toy\ProVerb\ProVerb.exe"; Tasks: desktopicon; Components: standalone

[Run]
Filename: "{autopf}\Producer Toy\ProVerb\ProVerb.exe"; Description: "Launch ProVerb Standalone"; Flags: nowait postinstall skipifsilent unchecked; Components: standalone

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
