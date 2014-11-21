!include "LogicLib.nsh"
!include "x64.nsh"
!include "FileFunc.nsh"
!insertmacro GetParameters
!insertmacro GetOptions


!define PROJECTNAME "Collectps - updated within grunt"
!define VERSION "version - updated within grunt"
!define OUTPUTFILE "${PROJECTNAME}-${VERSION}.install.exe"

Name "${PROJECTNAME}"

OutFile "${OUTPUTFILE}"

LicenseText "${PROJECTNAME} is distributed under the GNU General Public License." 
LicenseData "LICENSE" 

Function .onInit
  Var /GLOBAL cmdLineParams
  Var /GLOBAL option_INSTALLSERVICE
  Var /GLOBAL option_INSTALLDEFAULTCONFIGFILE

  StrCpy $option_INSTALLSERVICE 1
  StrCpy $option_INSTALLDEFAULTCONFIGFILE 1

  ${If} $InstDir == "" ; /D not used
    StrCpy $InstDir $programfiles32\${PROJECTNAME}
    ${If} ${RunningX64}
      StrCpy $InstDir $programfiles64\${PROJECTNAME}
    ${EndIf}
  ${EndIf}
  
  Push $R0
  ${GetParameters} $cmdLineParams
  ClearErrors
  ${GetOptions} $cmdLineParams '/?' $R0
  IfErrors +3 0
  MessageBox MB_OK "Command line options: $\r$\n\
    ${OUTPUTFILE} [/S] [options] [/D=<installation path>]$\r$\n\
  $\r$\n\
  /S :$\t$\t$\tsilent install$\r$\n\
  /nodefaultconfig : $\t$\tdo not install default configuration file$\r$\n\
  /noservice :$\t$\tdo not install the service$\r$\n\
  /D=<installation path> :$\tinstall to <installation path> (no quotes)$\r$\n\
  " /SD IDOK
  Abort
  Pop $R0
  
  ${GetOptions} $cmdLineParams /nodefaultconfig $R0
  IfErrors +2 0
  StrCpy $option_INSTALLDEFAULTCONFIGFILE 0
  
  ${GetOptions} $cmdLineParams /noservice $R0
  IfErrors +2 0
  StrCpy $option_INSTALLSERVICE 0
  
FunctionEnd

Page license
Page directory
Page components
Page instfiles

UninstPage uninstConfirm
UninstPage instfiles

SectionGroup /e "CollectPS"
  Section "core"
    SectionIn RO
    SetOutPath $InstDir
    File build\collectps.js
    File build\collectps_process.js
    File build\collectps_network.js
    File build\httpconfig.js
    File build\service.js
    File LICENSE
    SetOutPath $InstDir\bin
    File build\node.exe
    ${If} ${RunningX64}
          File /oname=node.exe build\node64.exe
    ${Else}
          File /oname=node.exe build\node32.exe
    ${EndIf}
    SetOutPath $InstDir\frontend
    File build/frontend\index.html
    File build/frontend\jquery-2.1.1.min.js
    File build/frontend\collectps.css
    SetOutPath $InstDir\config
    File /oname=default.json-dist config\default.json
    
    SetOutPath $InstDir\node_modules\body-parser
    File /r /x .git /x .gitignore /x .npmignore node_modules\body-parser\*.*
    SetOutPath $InstDir\node_modules\connect-basic-auth
    File /r /x .git /x .gitignore /x .npmignore node_modules\connect-basic-auth\*.*
    SetOutPath $InstDir\node_modules\express
    File /r /x .git /x .gitignore /x .npmignore node_modules\express\*.*
    SetOutPath $InstDir\node_modules\MD5
    File /r /x .git /x .gitignore /x .npmignore node_modules\MD5\*.*
    SetOutPath $InstDir\node_modules\node-windows
    File /r /x .git /x .gitignore /x .npmignore node_modules\node-windows\*.*
    SetOutPath $InstDir\node_modules\perfmon
    File /r /x .git /x .gitignore /x .npmignore node_modules\perfmon\*.*
    SetOutPath $InstDir\node_modules\config
    File /r /x .git /x .gitignore /x .npmignore node_modules\config\*.*
    SetOutPath $InstDir\node_modules\process
    File /r /x .git /x .gitignore /x .npmignore node_modules\process\*.*

    ; Write the uninstall keys for Windows
    WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PROJECTNAME}" "DisplayVersion" "${VERSION}"
    WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PROJECTNAME}" "DisplayName" "${PROJECTNAME} ${VERSION}"
    WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PROJECTNAME}" "UninstallString" '"$InstDir\uninstall.exe"'
    WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PROJECTNAME}" "Publisher" "Cyril Feraudet, https://github.com/perfwatcher/collectm"
    WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PROJECTNAME}" "HelpLink" "https://github.com/perfwatcher/collectm"
    WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PROJECTNAME}" "URLInfoAbout" "https://github.com/perfwatcher/collectm"
    WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PROJECTNAME}" "URLUpdateInfo" "https://github.com/perfwatcher/collectm/releases"
    WriteRegDWORD HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PROJECTNAME}" "NoModify" 1
    WriteRegDWORD HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PROJECTNAME}" "NoRepair" 1
    WriteUninstaller uninstall.exe
    

  SectionEnd

  Section "default configuration file"
	${If} $option_INSTALLDEFAULTCONFIGFILE == 1
      SetOutPath $InstDir\config
      File config\default.json
	${EndIf}
  SectionEnd
  Section "create and start the Collectps service"
    ${If} $option_INSTALLSERVICE == 1
      ExecWait '"$InstDir\bin\node.exe" "$InstDir\service.js" installAndStart'
	${EndIf}
  SectionEnd
SectionGroupEnd

Section "Uninstall"

  ExecWait '"$InstDir\bin\node.exe" "$InstDir\service.js" stopAndUninstall'

  DeleteRegKey HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PROJECTNAME}"

  RmDir /r $InstDir\node_modules
  RmDir /r $InstDir\bin
  RmDir /r $InstDir\daemon
  RmDir /r $InstDir\frontend
  Delete $InstDir\collectps.js
  Delete $InstDir\collectps_network.js
  Delete $InstDir\collectps_process.js
  Delete $InstDir\httpconfig.js
  Delete $InstDir\service.js
  Delete $InstDir\config\default.json-dist
  Delete $InstDir\LICENSE
  Delete $InstDir\uninstall.exe
  RmDir $InstDir\config
  RmDir $InstDir


SectionEnd
