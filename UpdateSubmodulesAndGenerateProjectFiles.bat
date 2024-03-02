@echo off

echo | call %~dp0\Submodules_InitializeAndUpdate.bat
echo | call %~dp0\GenerateProjectFiles.bat
echo | call %~dp0\GenerateProjectFilesVSCode.bat

pause