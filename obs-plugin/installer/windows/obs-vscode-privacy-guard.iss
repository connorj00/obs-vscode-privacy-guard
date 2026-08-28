#define AppName "OBS-VSCode Privacy Guard"
#define AppPublisher "Connor J Davies"
#define AppUrl "https://github.com/connorj00/obs-vscode-privacy-guard"

#ifndef AppVersion
	#define AppVersion "0.0.1"
#endif

#ifndef SourceDll
	#define SourceDll "..\..\build_x64\RelWithDebInfo\obs-vscode-privacy-guard.dll"
#endif

[Setup]
AppId={{94928A80-6A67-4385-9590-BAAC9D399525}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppUrl}
AppSupportURL={#AppUrl}/issues
AppUpdatesURL={#AppUrl}/releases
DefaultDirName={autopf}\obs-studio
DisableProgramGroupPage=yes
LicenseFile=..\..\LICENSE
SetupIconFile=assets\privacy-guard.ico
OutputDir=..\..\dist
OutputBaseFilename=obs-vscode-privacy-guard-setup-{#AppVersion}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
CloseApplications=no
RestartApplications=no
UninstallDisplayIcon={uninstallexe}
VersionInfoVersion={#AppVersion}
VersionInfoCompany={#AppPublisher}
VersionInfoDescription={#AppName} installer
VersionInfoProductName={#AppName}
VersionInfoProductVersion={#AppVersion}

[Files]
Source: "{#SourceDll}"; DestDir: "{app}\obs-plugins\64bit"; Flags: ignoreversion

[Code]
function IsObsRunning: Boolean;
var
	ResultCode: Integer;
begin
	Exec(
		ExpandConstant('{cmd}'),
		'/C tasklist /FI "IMAGENAME eq obs64.exe" /NH | find /I "obs64.exe" >NUL',
		'',
		SW_HIDE,
		ewWaitUntilTerminated,
		ResultCode
	);
	Result := ResultCode = 0;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
	Result := True;

	if (CurPageID = wpSelectDir) and
		(not DirExists(ExpandConstant('{app}\obs-plugins\64bit'))) then
	begin
		MsgBox(
			'Select the OBS Studio installation directory. The selected directory must contain obs-plugins\64bit.',
			mbError,
			MB_OK
		);
		Result := False;
		exit;
	end;

	if (CurPageID = wpReady) and IsObsRunning then
	begin
		MsgBox(
			'Close OBS Studio before installing or updating OBS-VSCode Privacy Guard, then try again.',
			mbError,
			MB_OK
		);
		Result := False;
	end;
end;

function InitializeUninstall: Boolean;
begin
	Result := not IsObsRunning;
	if not Result then
		MsgBox(
			'Close OBS Studio before uninstalling OBS-VSCode Privacy Guard, then try again.',
			mbError,
			MB_OK
		);
end;
