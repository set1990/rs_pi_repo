@echo off
echo File Synchronization
call "%~dp0build_serwer_sync.bat"
echo Building applications
C:\Windows\System32\OpenSSH\ssh.exe %*