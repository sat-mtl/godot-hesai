@echo on
setlocal

cd /d "%~dp0"

:: ===========================================
:: Set up MSVC 2022 Developer environment
:: ===========================================
echo ============================================
echo  Setting up MSVC 17 (Visual Studio 2022)
echo ============================================

if not defined VSINSTALLDIR (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" 2>nul
    call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" 2>nul
)

echo ============================================
echo  Compiler version:
cl
echo ============================================

:: =============================================
:: CMake configure
:: =============================================
echo ============================================
echo  Configuring project with CMake
echo ============================================

if "%BUILD_RELEASE%"=="" (
    cmake -B build -DCMAKE_POLICY_VERSION_MINIMUM="3.5"
) else (
    cmake -B build -DCMAKE_POLICY_VERSION_MINIMUM="3.5" -DGODOTCPP_TARGET="template_release"
)

if %ERRORLEVEL% neq 0 (
    echo Configuration failed with error code %ERRORLEVEL%.
    exit /b %ERRORLEVEL%
)

:: =============================================
:: Build
:: =============================================
echo ============================================
echo  Building project
echo ============================================

if "%BUILD_RELEASE%"=="" (
    cmake --build build
) else (
    cmake --build build --config Release
)

if %ERRORLEVEL% neq 0 (
    echo Build failed with error code %ERRORLEVEL%.
    exit /b %ERRORLEVEL%
)

echo ============================================
echo  Build completed successfully.
echo ============================================

exit /b 0
