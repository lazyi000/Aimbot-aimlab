@echo off
setlocal

rem ============================================================
rem  aimlab - build script (MinGW-w64 g++)
rem  Requisito: g++ do MinGW-w64 no PATH (ou em C:\mingw64\bin)
rem  Saida: aimlab.exe na raiz (sem DLLs externas)
rem ============================================================

if exist "C:\mingw64\bin\g++.exe" set "PATH=C:\mingw64\bin;%PATH%"

g++ -O2 -std=c++17 -Isrc -Ithird_party\imgui ^
    src\main.cpp ^
    src\capture\color_match.cpp src\capture\color_sample.cpp src\capture\screen_capture.cpp ^
    src\core\aim_worker.cpp ^
    src\input\hotkey.cpp src\input\memory_reader.cpp src\input\mouse_input.cpp ^
    src\ui\logo.cpp src\ui\window_picker.cpp ^
    third_party\imgui\imgui.cpp third_party\imgui\imgui_draw.cpp ^
    third_party\imgui\imgui_tables.cpp third_party\imgui\imgui_widgets.cpp ^
    third_party\imgui\imgui_impl_win32.cpp third_party\imgui\imgui_impl_opengl3.cpp ^
    -lopengl32 -lgdi32 -luser32 -ldwmapi -lole32 -lwindowscodecs ^
    -static-libgcc -static-libstdc++ -mwindows ^
    -o aimlab.exe

if %errorlevel% equ 0 (
    echo [OK] aimlab.exe compilado
) else (
    echo [ERRO] Compilacao falhou
)

endlocal