; Instalador do ReplayDeck (Inno Setup 6)
; Compilado no GitHub Actions:  iscc /DMyVersion=1.0.0 /DSrcDir=<pasta do plugin> replaydeck.iss

#ifndef MyVersion
  #define MyVersion "1.0.0"
#endif
#ifndef SrcDir
  ; SrcDir\obs30 .. obs33\replaydeck.dll (uma DLL por versão do OBS) e SrcDir\data
  #define SrcDir "..\pkg-win"
#endif
#define MyName "ReplayDeck"
#define PluginFolder "replaydeck"
#define OldFolder "obs-instant-replay"

[Setup]
AppId={{6B1F2D4E-7A3C-4E59-9C1B-0D2E5F7A8B91}
AppName={#MyName}
AppVersion={#MyVersion}
AppVerName={#MyName} {#MyVersion}
AppPublisher=ReplayDeck
DefaultDirName={code:GetPluginDir}
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableReadyMemo=no
UsePreviousAppDir=no
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputBaseFilename=replaydeck-{#MyVersion}-windows-installer
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName={#MyName}
CloseApplications=no
ShowLanguageDialog=auto

[Languages]
; abre no idioma do Windows (Configurações do Windows → Idioma)
Name: "en"; MessagesFile: "compiler:Default.isl"
Name: "pt"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"
Name: "es"; MessagesFile: "compiler:Languages\Spanish.isl"
Name: "fr"; MessagesFile: "compiler:Languages\French.isl"
Name: "de"; MessagesFile: "compiler:Languages\German.isl"
Name: "it"; MessagesFile: "compiler:Languages\Italian.isl"
Name: "ru"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "ja"; MessagesFile: "compiler:Languages\Japanese.isl"
#if FileExists(AddBackslash(CompilerPath) + "Languages\Korean.isl")
Name: "ko"; MessagesFile: "compiler:Languages\Korean.isl"
#endif
#if FileExists(AddBackslash(CompilerPath) + "Languages\ChineseSimplified.isl")
Name: "zh"; MessagesFile: "compiler:Languages\ChineseSimplified.isl"
#endif

[Files]
; o OBS recusa plugin compilado para versão mais nova que ele: instala a DLL da versão do OBS encontrado
Source: "{#SrcDir}\obs30\{#PluginFolder}.dll"; DestDir: "{code:GetBinDir}"; Flags: ignoreversion; Check: UseBuild(30)
Source: "{#SrcDir}\obs31\{#PluginFolder}.dll"; DestDir: "{code:GetBinDir}"; Flags: ignoreversion; Check: UseBuild(31)
Source: "{#SrcDir}\obs32\{#PluginFolder}.dll"; DestDir: "{code:GetBinDir}"; Flags: ignoreversion; Check: UseBuild(32)
Source: "{#SrcDir}\obs33\{#PluginFolder}.dll"; DestDir: "{code:GetBinDir}"; Flags: ignoreversion; Check: UseBuild(33)
Source: "{#SrcDir}\data\*"; DestDir: "{code:GetDataDir}"; Flags: ignoreversion recursesubdirs createallsubdirs

[InstallDelete]
; jeitos antigos de instalar (Instant Replay e ReplayDeck 1.0.0): tirar para o OBS não carregar dois
Type: filesandordirs; Name: "{code:OldPath|1}"
Type: filesandordirs; Name: "{code:OldPath|2}"
Type: filesandordirs; Name: "{code:OldPath|3}"
Type: filesandordirs; Name: "{code:OldPath|4}"
Type: filesandordirs; Name: "{code:OldPath|5}"
Type: filesandordirs; Name: "{code:OldPath|6}"

[UninstallDelete]
Type: dirifempty; Name: "{code:GetDataDir}"
Type: dirifempty; Name: "{app}"

[CustomMessages]
en.ObsFound=OBS found at: %1
en.ObsNone=No regular OBS installation was found on this PC.
en.ModeTitle=Where is your OBS?
en.ModeDesc=Choose the type of OBS the plugin will be installed in. Close OBS before continuing.
en.ModeInstalled=OBS installed normally (recommended)
en.ModePortable=Portable OBS (a folder with OBS, e.g. C:\OBS-Test)
en.PortTitle=Portable OBS folder
en.PortSub=Choose the main folder of the portable OBS
en.PortDesc=It is the folder that contains "bin", "data" and "config" (obs64.exe is in bin\64bit).
en.NoObs1=OBS was not found in this folder (bin\64bit\obs64.exe is missing).
en.NoObs2=Choose the main folder of the portable OBS.
en.MemoAt=The plugin will be installed in:
en.MemoDocks=Then just open OBS: the panels are in Docks →
en.MemoList=ReplayDeck, ReplayDeck (overlays) and ReplayDeck GT (titles).
en.ObsTooOld=This plugin needs OBS Studio 30 or newer. OBS found: %1

pt.ObsFound=OBS encontrado em: %1
pt.ObsNone=Nenhuma instalação normal do OBS foi encontrada neste PC.
pt.ModeTitle=Onde está o seu OBS?
pt.ModeDesc=Escolha o tipo de OBS em que o plugin será instalado. Feche o OBS antes de continuar.
pt.ModeInstalled=OBS instalado normalmente (recomendado)
pt.ModePortable=OBS portátil (uma pasta com o OBS, ex.: C:\OBS-Teste)
pt.PortTitle=Pasta do OBS portátil
pt.PortSub=Escolha a pasta principal do OBS portátil
pt.PortDesc=É a pasta que contém "bin", "data" e "config" (o obs64.exe fica em bin\64bit).
pt.NoObs1=Não encontrei o OBS nesta pasta (falta bin\64bit\obs64.exe).
pt.NoObs2=Escolha a pasta principal do OBS portátil.
pt.MemoAt=O plugin será instalado em:
pt.MemoDocks=Depois é só abrir o OBS: os painéis ficam em Painéis (Docks) →
pt.MemoList=ReplayDeck, ReplayDeck (overlays) e ReplayDeck GT (títulos).
pt.ObsTooOld=Este plugin precisa do OBS Studio 30 ou mais novo. OBS encontrado: %1

es.ObsFound=OBS encontrado en: %1
es.ObsNone=No se encontró una instalación normal de OBS en esta PC.
es.ModeTitle=¿Dónde está tu OBS?
es.ModeDesc=Elige el tipo de OBS donde se instalará el plugin. Cierra OBS antes de continuar.
es.ModeInstalled=OBS instalado normalmente (recomendado)
es.ModePortable=OBS portátil (una carpeta con OBS, ej.: C:\OBS-Prueba)
es.PortTitle=Carpeta del OBS portátil
es.PortSub=Elige la carpeta principal del OBS portátil
es.PortDesc=Es la carpeta que contiene "bin", "data" y "config" (obs64.exe está en bin\64bit).
es.NoObs1=No se encontró OBS en esta carpeta (falta bin\64bit\obs64.exe).
es.NoObs2=Elige la carpeta principal del OBS portátil.
es.MemoAt=El plugin se instalará en:
es.MemoDocks=Luego abre OBS: los paneles están en Paneles (Docks) →
es.MemoList=ReplayDeck, ReplayDeck (overlays) y ReplayDeck GT (títulos).
es.ObsTooOld=Este plugin necesita OBS Studio 30 o más reciente. OBS encontrado: %1

fr.ObsFound=OBS trouvé dans : %1
fr.ObsNone=Aucune installation normale d'OBS n'a été trouvée sur ce PC.
fr.ModeTitle=Où est votre OBS ?
fr.ModeDesc=Choisissez le type d'OBS où le plugin sera installé. Fermez OBS avant de continuer.
fr.ModeInstalled=OBS installé normalement (recommandé)
fr.ModePortable=OBS portable (un dossier avec OBS, ex. : C:\OBS-Test)
fr.PortTitle=Dossier de l'OBS portable
fr.PortSub=Choisissez le dossier principal de l'OBS portable
fr.PortDesc=C'est le dossier qui contient "bin", "data" et "config" (obs64.exe est dans bin\64bit).
fr.NoObs1=OBS introuvable dans ce dossier (bin\64bit\obs64.exe manquant).
fr.NoObs2=Choisissez le dossier principal de l'OBS portable.
fr.MemoAt=Le plugin sera installé dans :
fr.MemoDocks=Ensuite, ouvrez OBS : les panneaux sont dans Docks →
fr.MemoList=ReplayDeck, ReplayDeck (overlays) et ReplayDeck GT (titres).
fr.ObsTooOld=Ce plugin nécessite OBS Studio 30 ou plus récent. OBS trouvé : %1

de.ObsFound=OBS gefunden in: %1
de.ObsNone=Auf diesem PC wurde keine normale OBS-Installation gefunden.
de.ModeTitle=Wo ist Ihr OBS?
de.ModeDesc=Wählen Sie das OBS, in das das Plugin installiert wird. Schließen Sie OBS vorher.
de.ModeInstalled=Normal installiertes OBS (empfohlen)
de.ModePortable=Portables OBS (ein Ordner mit OBS, z. B. C:\OBS-Test)
de.PortTitle=Ordner des portablen OBS
de.PortSub=Wählen Sie den Hauptordner des portablen OBS
de.PortDesc=Das ist der Ordner mit "bin", "data" und "config" (obs64.exe liegt in bin\64bit).
de.NoObs1=In diesem Ordner wurde kein OBS gefunden (bin\64bit\obs64.exe fehlt).
de.NoObs2=Wählen Sie den Hauptordner des portablen OBS.
de.MemoAt=Das Plugin wird installiert in:
de.MemoDocks=Dann OBS öffnen: Die Panels sind unter Docks →
de.MemoList=ReplayDeck, ReplayDeck (Overlays) und ReplayDeck GT (Titel).
de.ObsTooOld=Dieses Plugin braucht OBS Studio 30 oder neuer. Gefundenes OBS: %1

it.ObsFound=OBS trovato in: %1
it.ObsNone=Nessuna installazione normale di OBS trovata su questo PC.
it.ModeTitle=Dove si trova il tuo OBS?
it.ModeDesc=Scegli il tipo di OBS in cui installare il plugin. Chiudi OBS prima di continuare.
it.ModeInstalled=OBS installato normalmente (consigliato)
it.ModePortable=OBS portatile (una cartella con OBS, es.: C:\OBS-Test)
it.PortTitle=Cartella dell'OBS portatile
it.PortSub=Scegli la cartella principale dell'OBS portatile
it.PortDesc=È la cartella che contiene "bin", "data" e "config" (obs64.exe è in bin\64bit).
it.NoObs1=OBS non trovato in questa cartella (manca bin\64bit\obs64.exe).
it.NoObs2=Scegli la cartella principale dell'OBS portatile.
it.MemoAt=Il plugin verrà installato in:
it.MemoDocks=Poi apri OBS: i pannelli sono in Pannelli (Docks) →
it.MemoList=ReplayDeck, ReplayDeck (overlay) e ReplayDeck GT (titoli).
it.ObsTooOld=Questo plugin richiede OBS Studio 30 o più recente. OBS trovato: %1

ru.ObsFound=OBS найден: %1
ru.ObsNone=Обычная установка OBS на этом ПК не найдена.
ru.ModeTitle=Где ваш OBS?
ru.ModeDesc=Выберите, в какой OBS установить плагин. Закройте OBS перед продолжением.
ru.ModeInstalled=OBS, установленный обычным способом (рекомендуется)
ru.ModePortable=Портативный OBS (папка с OBS, напр. C:\OBS-Test)
ru.PortTitle=Папка портативного OBS
ru.PortSub=Выберите главную папку портативного OBS
ru.PortDesc=Это папка, где есть "bin", "data" и "config" (obs64.exe лежит в bin\64bit).
ru.NoObs1=В этой папке нет OBS (нет bin\64bit\obs64.exe).
ru.NoObs2=Выберите главную папку портативного OBS.
ru.MemoAt=Плагин будет установлен в:
ru.MemoDocks=Затем откройте OBS: панели находятся в меню Доки →
ru.MemoList=ReplayDeck, ReplayDeck (оверлеи) и ReplayDeck GT (титры).
ru.ObsTooOld=Этому плагину нужен OBS Studio 30 или новее. Найден OBS: %1

ja.ObsFound=OBS の場所: %1
ja.ObsNone=この PC に通常インストールされた OBS が見つかりません。
ja.ModeTitle=OBS はどこにありますか？
ja.ModeDesc=プラグインをインストールする OBS の種類を選んでください。続ける前に OBS を閉じてください。
ja.ModeInstalled=通常インストールの OBS（おすすめ）
ja.ModePortable=ポータブル版 OBS（OBS が入ったフォルダ、例: C:\OBS-Test）
ja.PortTitle=ポータブル版 OBS のフォルダ
ja.PortSub=ポータブル版 OBS のメインフォルダを選んでください
ja.PortDesc="bin"、"data"、"config" があるフォルダです（obs64.exe は bin\64bit にあります）。
ja.NoObs1=このフォルダに OBS が見つかりません（bin\64bit\obs64.exe がありません）。
ja.NoObs2=ポータブル版 OBS のメインフォルダを選んでください。
ja.MemoAt=プラグインのインストール先:
ja.MemoDocks=あとは OBS を開くだけ: パネルは ドック →
ja.MemoList=ReplayDeck、ReplayDeck（オーバーレイ）、ReplayDeck GT（テロップ）
ja.ObsTooOld=このプラグインには OBS Studio 30 以降が必要です。見つかった OBS: %1

#if FileExists(AddBackslash(CompilerPath) + "Languages\\Korean.isl")
ko.ObsFound=OBS 위치: %1
ko.ObsNone=이 PC에서 일반 설치된 OBS를 찾지 못했습니다.
ko.ModeTitle=OBS가 어디에 있나요?
ko.ModeDesc=플러그인을 설치할 OBS 종류를 선택하세요. 계속하기 전에 OBS를 닫으세요.
ko.ModeInstalled=일반 설치된 OBS (추천)
ko.ModePortable=포터블 OBS (OBS가 들어 있는 폴더, 예: C:\OBS-Test)
ko.PortTitle=포터블 OBS 폴더
ko.PortSub=포터블 OBS의 기본 폴더를 선택하세요
ko.PortDesc="bin", "data", "config"가 있는 폴더입니다 (obs64.exe는 bin\64bit에 있음).
ko.NoObs1=이 폴더에서 OBS를 찾지 못했습니다 (bin\64bit\obs64.exe 없음).
ko.NoObs2=포터블 OBS의 기본 폴더를 선택하세요.
ko.MemoAt=플러그인 설치 위치:
ko.MemoDocks=이제 OBS를 열면 됩니다: 패널은 독 →
ko.MemoList=ReplayDeck, ReplayDeck (오버레이), ReplayDeck GT (타이틀)
ko.ObsTooOld=이 플러그인은 OBS Studio 30 이상이 필요합니다. 찾은 OBS: %1
#endif

#if FileExists(AddBackslash(CompilerPath) + "Languages\\ChineseSimplified.isl")
zh.ObsFound=已找到 OBS：%1
zh.ObsNone=此电脑上没有找到正常安装的 OBS。
zh.ModeTitle=你的 OBS 在哪里？
zh.ModeDesc=选择要安装插件的 OBS 类型。继续之前请关闭 OBS。
zh.ModeInstalled=正常安装的 OBS（推荐）
zh.ModePortable=便携版 OBS（包含 OBS 的文件夹，例如 C:\OBS-Test）
zh.PortTitle=便携版 OBS 文件夹
zh.PortSub=选择便携版 OBS 的主文件夹
zh.PortDesc=即包含 "bin"、"data" 和 "config" 的文件夹（obs64.exe 位于 bin\64bit）。
zh.NoObs1=此文件夹中没有找到 OBS（缺少 bin\64bit\obs64.exe）。
zh.NoObs2=请选择便携版 OBS 的主文件夹。
zh.MemoAt=插件将安装到：
zh.MemoDocks=然后打开 OBS：面板位于 停靠窗口 →
zh.MemoList=ReplayDeck、ReplayDeck（叠加层）和 ReplayDeck GT（字幕）
zh.ObsTooOld=此插件需要 OBS Studio 30 或更新版本。找到的 OBS：%1
#endif

[Code]
var
  ModePage: TInputOptionWizardPage;
  PortablePage: TInputDirWizardPage;

function ObsInstalledDir(): String;
begin
  Result := '';
  if not RegQueryStringValue(HKLM64, 'SOFTWARE\OBS Studio', '', Result) then
    RegQueryStringValue(HKLM, 'SOFTWARE\OBS Studio', '', Result);
end;

function GetPluginDir(Param: String): String;
begin
  if (ModePage <> nil) and (ModePage.SelectedValueIndex = 1) then
    { OBS portátil: a DLL vai em <pasta do OBS>\obs-plugins\64bit }
    Result := AddBackslash(PortablePage.Values[0]) + 'data\obs-plugins\{#PluginFolder}'
  else
    { OBS instalado: C:\ProgramData\obs-studio\plugins\replaydeck }
    Result := ExpandConstant('{commonappdata}\obs-studio\plugins\{#PluginFolder}');
end;

function IsPortable(): Boolean;
begin
  Result := (ModePage <> nil) and (ModePage.SelectedValueIndex = 1);
end;

function PortableRoot(): String;
begin
  Result := AddBackslash(PortablePage.Values[0]);
end;

{ jeito de instalar que o OBS 28 até o 33 entendem }
function GetBinDir(Param: String): String;
begin
  if IsPortable() then
    Result := PortableRoot() + 'obs-plugins\64bit'
  else
    Result := ExpandConstant('{commonappdata}\obs-studio\plugins\{#PluginFolder}\bin\64bit');
end;

function GetDataDir(Param: String): String;
begin
  if IsPortable() then
    Result := PortableRoot() + 'data\obs-plugins\{#PluginFolder}'
  else
    Result := ExpandConstant('{commonappdata}\obs-studio\plugins\{#PluginFolder}\data');
end;

{ instalações antigas a remover; caminho que não existe quando não se aplica }
function OldPath(Param: String): String;
var
  pd: String;
begin
  Result := ExpandConstant('{tmp}\nada-' + Param);
  pd := ExpandConstant('{commonappdata}\obs-studio\plugins\');
  if IsPortable() then begin
    if Param = '1' then Result := PortableRoot() + 'plugins\{#OldFolder}';
    if Param = '2' then Result := PortableRoot() + 'plugins\{#PluginFolder}';
    if Param = '3' then Result := PortableRoot() + 'obs-plugins\64bit\{#OldFolder}.dll';
    if Param = '4' then Result := PortableRoot() + 'data\obs-plugins\{#OldFolder}';
  end else begin
    if Param = '1' then Result := pd + '{#OldFolder}';
    if Param = '2' then Result := pd + '{#PluginFolder}\{#PluginFolder}.dll';
  end;
end;

{ versão principal do OBS escolhido (30, 31...), 0 se não deu para ler }
function ObsMajor(): Integer;
var
  exe, v: String;
  p: Integer;
begin
  Result := 0;
  if IsPortable() then
    exe := PortableRoot() + 'bin\64bit\obs64.exe'
  else
    exe := AddBackslash(ObsInstalledDir()) + 'bin\64bit\obs64.exe';
  if GetVersionNumbersString(exe, v) then begin
    p := Pos('.', v);
    if p > 0 then
      Result := StrToIntDef(Copy(v, 1, p - 1), 0);
  end;
end;

function ObsVersionText(): String;
var
  exe, v: String;
begin
  Result := '?';
  if IsPortable() then
    exe := PortableRoot() + 'bin\64bit\obs64.exe'
  else
    exe := AddBackslash(ObsInstalledDir()) + 'bin\64bit\obs64.exe';
  if GetVersionNumbersString(exe, v) then
    Result := v;
end;

{ qual DLL usar: a da versão do OBS; mais novo que 33 ou desconhecido usa a do 33 }
function UseBuild(Major: Integer): Boolean;
var
  m: Integer;
begin
  m := ObsMajor();
  if (m <= 0) or (m > 33) then
    m := 33;
  Result := (m = Major);
end;

procedure InitializeWizard;
var
  hint: String;
begin
  if ObsInstalledDir() <> '' then
    hint := FmtMessage(CustomMessage('ObsFound'), [ObsInstalledDir()])
  else
    hint := CustomMessage('ObsNone');

  ModePage := CreateInputOptionPage(wpWelcome,
    CustomMessage('ModeTitle'), hint,
    CustomMessage('ModeDesc'),
    True, False);
  ModePage.Add(CustomMessage('ModeInstalled'));
  ModePage.Add(CustomMessage('ModePortable'));
  if ObsInstalledDir() <> '' then
    ModePage.SelectedValueIndex := 0
  else
    ModePage.SelectedValueIndex := 1;

  PortablePage := CreateInputDirPage(ModePage.ID,
    CustomMessage('PortTitle'), CustomMessage('PortSub'),
    CustomMessage('PortDesc'),
    False, '');
  PortablePage.Add('');
  PortablePage.Values[0] := 'C:\OBS-Studio';
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := (PageID = PortablePage.ID) and (ModePage.SelectedValueIndex = 0);
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if (CurPageID = ModePage.ID) or (CurPageID = PortablePage.ID) then
    WizardForm.DirEdit.Text := GetPluginDir('');
  if (CurPageID = PortablePage.ID) and
     not FileExists(AddBackslash(PortablePage.Values[0]) + 'bin\64bit\obs64.exe') then
  begin
    MsgBox(CustomMessage('NoObs1') + #13#10 + CustomMessage('NoObs2'), mbError, MB_OK);
    Result := False;
  end;
  if Result and ((CurPageID = PortablePage.ID) or ((CurPageID = ModePage.ID) and not IsPortable())) then
    if (ObsMajor() > 0) and (ObsMajor() < 30) then begin
      MsgBox(FmtMessage(CustomMessage('ObsTooOld'), [ObsVersionText()]), mbError, MB_OK);
      Result := False;
    end;
end;

function UpdateReadyMemo(Space, NewLine, MemoUserInfoInfo, MemoDirInfo, MemoTypeInfo,
  MemoComponentsInfo, MemoGroupInfo, MemoTasksInfo: String): String;
begin
  Result := CustomMessage('MemoAt') + NewLine + Space + GetBinDir('') + NewLine + Space + GetDataDir('') + NewLine + Space + 'OBS ' + ObsVersionText() + NewLine + NewLine +
            CustomMessage('MemoDocks') + NewLine + Space + CustomMessage('MemoList');
end;
