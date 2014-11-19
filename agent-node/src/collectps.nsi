Name "CollectPS"

OutFile "out.exe"

InstallDir $PROGRAMFILES\CollectPS

Page directory
Page instfiles

UninstPage uninstConfirm
UninstPage instfiles

Section ""
	SetOutPath $INSTDIR
	File build\collectps.js
	File build\httpconfig.js
	File build\collectps_process.js
	File build\collectps_network.js
	File build\service.js
	SetOutPath $INSTDIR\bin
	File build\node.exe
	SetOutPath $INSTDIR\config
	File config\default.json
	SetOutPath $INSTDIR\frontend
	File build/frontend\index.html
	File build/frontend\jquery-2.1.1.min.js
	File build/frontend\collectps.css
	
	SetOutPath $INSTDIR\node_modules\body-parser
	File /r /x .git /x .gitignore /x .npmignore node_modules\body-parser\*.*
	SetOutPath $INSTDIR\node_modules\connect-basic-auth
	File /r /x .git /x .gitignore /x .npmignore node_modules\connect-basic-auth\*.*
	SetOutPath $INSTDIR\node_modules\express
	File /r /x .git /x .gitignore /x .npmignore node_modules\express\*.*
	SetOutPath $INSTDIR\node_modules\MD5
	File /r /x .git /x .gitignore /x .npmignore node_modules\MD5\*.*
	SetOutPath $INSTDIR\node_modules\node-windows
	File /r /x .git /x .gitignore /x .npmignore node_modules\node-windows\*.*
	SetOutPath $INSTDIR\node_modules\perfmon
	File /r /x .git /x .gitignore /x .npmignore node_modules\perfmon\*.*
	SetOutPath $INSTDIR\node_modules\config
	File /r /x .git /x .gitignore /x .npmignore node_modules\config\*.*

	WriteUninstaller uninstall.exe

	ExecWait '"$INSTDIR\bin\node.exe" "$INSTDIR\service.js" installAndStart'
	
SectionEnd

Section "Uninstall"

	ExecWait '"$INSTDIR\bin\node.exe" "$INSTDIR\service.js" stopAndUninstall'

	RmDir /r $INSTDIR\node_modules
	RmDir /r $INSTDIR\bin
	RmDir /r $INSTDIR\daemon
	RmDir /r $INSTDIR\frontend
	Delete $INSTDIR\collectps.js
	Delete $INSTDIR\httpconfig.js
	Delete $INSTDIR\service.js
	Delete $INSTDIR\config\default.json
	Delete $INSTDIR\uninstall.exe
	RmDir $INSTDIR\config
	RmDir $INSTDIR


SectionEnd
