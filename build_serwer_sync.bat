@echo off
set WINSCP_PATH="C:\Program Files (x86)\WinSCP\WinSCP.com"
set LOCAL_PATH="C:\rs_pi_repo\intercom_gpio_demon"
set REMOTE_PATH="/home/set1990/RSpi_project/intercom_gpio_demon"

%WINSCP_PATH% /log="C:\rs_pi_repo\sync_log.txt" /command ^
    "open sftp://set1990@192.168.1.117/ -privatekey=""C:\rs_pi_repo\build_serwer_key.ppk"" -hostkey=""*""" ^
    "synchronize both -filemask=""| .*/; .*"" %LOCAL_PATH% %REMOTE_PATH%" ^
    "exit"


set LOCAL_PATH="C:\rs_pi_repo\intercom_ipc_lib"
set REMOTE_PATH="/home/set1990/RSpi_project/intercom_ipc_lib"
%WINSCP_PATH% /log="C:\rs_pi_repo\sync_log.txt" /command ^
    "open sftp://set1990@192.168.1.117/ -privatekey=""C:\rs_pi_repo\build_serwer_key.ppk"" -hostkey=""*""" ^
    "synchronize both -filemask=""| .*/; .*"" %LOCAL_PATH% %REMOTE_PATH%" ^
    "exit"



echo Synchronization complete!

