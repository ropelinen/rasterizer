SET msbuild="C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild.exe"
SET solution=%1
SET platform=%2

%msbuild% %solution% /t:rebuild /p:Configuration=Debug;Platform=%platform%
if errorlevel 1 exit /B 1
%msbuild% %solution% /t:rebuild /p:Configuration=Release;Platform=%platform%
if errorlevel 1 exit /B 1
%msbuild% %solution% /t:rebuild /p:Configuration=Production;Platform=%platform%
if errorlevel 1 exit /B 1

exit /B 0
