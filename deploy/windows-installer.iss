; Inno Setup Script for Cool Live Caption
; Requires Inno Setup 6.0 or later: https://jrsoftware.org/isinfo.php

#define AppName "Cool Live Caption"
#define AppVersion "0.1.0"
#define AppPublisher "Luca Jones"
#define AppExeName "coollivecaption.exe"

[Setup]
AppId={{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={localappdata}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
OutputDir=../build/installer_output
OutputBaseFilename=CoolLiveCaptionSetup
Compression=lzma
SolidCompression=yes
WizardStyle=modern dynamic
DisableWelcomePage=no
UninstallDisplayIcon={app}\{#AppExeName}
LicenseFile=..\LICENSE

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Messages]
WelcomeLabel2=This will install [name/ver] on your computer. 
  
[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional icons:"

[Files]
Source: "..\\build\\bin\\{#AppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\\build\\bin\\*.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\\build\\bin\\profanity\\*"; DestDir: "{app}\\profanity"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\\resources\\icon.ico"; DestDir: "{app}"; Flags: ignoreversion; Check: FileExists(ExpandConstant('{#SourcePath}\\..\\resources\\icon.ico'))
Source: "..\\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\\THIRD-PARTY-LICENSES.md"; DestDir: "{app}"; Flags: ignoreversion; Check: FileExists(ExpandConstant('{#SourcePath}\\..\\THIRD-PARTY-LICENSES.md'))

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExeName}"
Name: "{group}\Models Folder"; Filename: "{localappdata}\coollivecaption\models"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExeName}"; Description: "Launch {#AppName}"; Flags: nowait postinstall skipifsilent

[Code]
procedure InitializeWizard;
var
  ModelsDir: String;
begin
  ModelsDir := ExpandConstant('{localappdata}\coollivecaption\models');
  if not DirExists(ModelsDir) then
    CreateDir(ModelsDir);
end;
