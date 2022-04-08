echo | set /p dummyName=#define CANDELA_COMMIT > Version\AutoVersion.h
git rev-parse --verify HEAD >> Version\AutoVersion.h
echo | set /p dummyName=#define CANDELA_DATE >> Version\AutoVersion.h
git log -1 --format=%%ci >> Version\AutoVersion.h
git diff --quiet || echo #define CANDELA_DIRTY >> Version\AutoVersion.h
exit /b 0
