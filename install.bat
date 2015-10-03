@echo off
REM stolen from lagarith

Set RegQry=HKLM\Hardware\Description\System\CentralProcessor\0
REM set OLDDIR=%CD%
pushd %~dp0%

REG.exe Query %RegQry% > checkOS.txt
echo Using as install path: "%~dp0%Runtimes"
echo Add install path for current user (64bit or 32bit systems only)...
REG.exe Add HKCU\Software\TestAMFVFW /v InstallPath  /t REG_SZ /f /d "%~dp0%Runtimes"
echo Add install path for local machine (64bit or 32bit systems only)...
REG.exe Add HKLM\Software\TestAMFVFW /v InstallPath  /t REG_SZ /f /d "%~dp0%Runtimes"
echo Add install path for current user (32bit on 64bit machine)...
%systemroot%\SysWOW64\REG.exe Add HKCU\Software\TestAMFVFW /v InstallPath  /t REG_SZ /f /d "%~dp0%Runtimes"
echo Add install path for local machine (32bit on 64bit machine)...
%systemroot%\SysWOW64\REG.exe Add HKLM\Software\TestAMFVFW /v InstallPath  /t REG_SZ /f /d "%~dp0%Runtimes"
 
Find /i "x86" < CheckOS.txt > StringCheck.txt
 
If %ERRORLEVEL% == 0 (
	del StringCheck.txt
	del CheckOS.txt 
	Echo "32 Bit Operating system detected, installing 32 bit TestAMFVFW version"
	copy TestAMFVFW.inf %windir%\system32\
	copy Bin32\TestAMFVFW.DLL %windir%\system32\

	cd /d %windir%\system32\
	rundll32 setupapi.dll,InstallHinfSection DefaultInstall 0 %windir%\system32\TestAMFVFW.inf
) ELSE (
	del StringCheck.txt
	del CheckOS.txt 

	echo ===
	echo === 64 Bit Operating System detected.
	echo ===

	REM (With how currently INF is set up) setupapi seems to look for DLLs in the same folder as INF.
	REM So just copy DLLs (aka do the installer's work twice :P) and run INF from dest. dir
	REM Copy INF for uninstaller. INF is also copied to %windir%\inf, but first run uninstaller probably removes that.
	copy TestAMFVFW.inf %windir%\system32\
	copy Bin64\TestAMFVFW.DLL %windir%\system32\
	
	copy TestAMFVFW.inf %windir%\SysWOW64\
	copy Bin32\TestAMFVFW.DLL %windir%\SysWOW64\
	
	rundll32 setupapi.dll,InstallHinfSection DefaultInstall 0 %windir%\System32\TestAMFVFW.inf
	
	REM Because Windows-On-Windows, you have to run this from within syswow64 dir
	REM so that windows would know it is 32bit version.
	cd /d %windir%\SysWOW64\
	rundll32 setupapi.dll,InstallHinfSection DefaultInstall 0 %windir%\SYSWOW64\TestAMFVFW.inf
)

popd
pause