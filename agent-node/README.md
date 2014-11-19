CollectPS
============

Service that watch processes and send statistics on them

Installation
============

* Download latest installer on https://github.com/ymettier/collectps/raw/master/releases/
* run the installer

CollectPS would be added as service and started. If not :
```
C:\Program\ Files\collectps\bin\node.exe C:\Program\ Files\collectps\service.js [install|installAndStart|uninstall|stopAndUninstall|start|stop]
```
or
```
C:\Program\ Files (x86)\collectps\bin\node.exe C:\Program\ Files (x86)\collectps\service.js [install|installAndStart|uninstall|stopAndUninstall|start|stop]
```

Installer options :
* /S : silent install
* /D=&lt;C:\your\path&gt; : install to C:\your\path

Example : install to C:\Program Files\collectps

```
collectps-<version>.exe /S /D=C:\Program Files\collectps
```

Configure
=========

Use your browser to go to http://localhost:28230/ (login: admin / password: admin)

Development
===========
Do not forget to install modules :
```
npm install --dev
```

Build with `grunt test`
Build the Windows installer with `grunt distexe`

FAQ
===
* Which Windows versions are suported ? Don't know yet
