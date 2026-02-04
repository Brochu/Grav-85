@echo off
odin build . -out:lvledit.exe
if %ERRORLEVEL% EQU 0 (
    echo Build successful: lvledit.exe
) else (
    echo Build failed
)
