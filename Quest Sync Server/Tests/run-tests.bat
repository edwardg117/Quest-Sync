@echo off
echo Running Quest Sync Server Tests...

REM Create results directory if it doesn't exist
if not exist results mkdir results

REM Get current date and time for filename using simpler method
set "timestamp=%date:~-4,4%-%date:~-7,2%-%date:~-10,2%_%time:~0,2%-%time:~3,2%-%time:~6,2%"
REM Remove spaces from timestamp (in case hours are single digit)
set "timestamp=%timestamp: =0%"

REM Check if we're in the right directory
if exist "Debug\QuestSyncServerTests.exe" (
    set "EXEPATH=Debug\QuestSyncServerTests.exe"
) else if exist "Release\QuestSyncServerTests.exe" (
    set "EXEPATH=Release\QuestSyncServerTests.exe"
) else if exist "x64\Debug\QuestSyncServerTests.exe" (
    set "EXEPATH=x64\Debug\QuestSyncServerTests.exe"
) else if exist "x64\Release\QuestSyncServerTests.exe" (
    set "EXEPATH=x64\Release\QuestSyncServerTests.exe"
) else if exist "QuestSyncServerTests.exe" (
    set "EXEPATH=QuestSyncServerTests.exe"
) else (
    echo ERROR: Could not find QuestSyncServerTests.exe
    echo Please build the solution first or run this script from the correct directory.
    echo.
    echo Press any key to exit...
    pause > nul
    exit /b 1
)

echo Found executable at: %EXEPATH%
echo Running tests...

REM Run tests with XML output
"%EXEPATH%" --output-xml=results\test-results-%timestamp%.xml

REM Check the results
if %ERRORLEVEL% EQU 0 (
    echo Tests completed successfully.
    echo Results saved to results\test-results-%timestamp%.xml
) else (
    echo Tests failed with error code %ERRORLEVEL%.
    echo Results saved to results\test-results-%timestamp%.xml
)

echo.
echo Press any key to exit...
pause > nul
