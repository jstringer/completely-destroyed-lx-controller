@echo off
setlocal
set PYTHONPATH=
set PYTHONHOME=
set "python=%~dp0\..\..\thirdparty\python\msvc\x86_64\python"
set "TARGET_DIR=%~dp0"
set "TARGET_DIR=%TARGET_DIR:~0,-1%"
"%python%" "%~dp0\..\..\tools\buildsystem\common\regenerate_app_by_dir.py" "%TARGET_DIR%" %*
