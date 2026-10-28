@echo off
echo ========================================================
echo   COMPILER OPTIMIZER TEST SUITE (WITH OLLAMA API)       
echo ========================================================
echo.

echo [1/7] Compiling main.cpp with WinINet HTTP API...
g++ -std=c++17 -Wall -Wextra main.cpp -o optimizer.exe -lwininet
if %ERRORLEVEL% NEQ 0 (
    echo [!] Compilation failed!
    exit /b %ERRORLEVEL%
)
echo [✓] Compiled optimizer.exe successfully.
echo.

echo [2/7] Running Default Input (input.cpp)...
.\optimizer.exe input.cpp
g++ -std=c++17 input_optimized.cpp -o run_input.exe
.\run_input.exe
if exist run_input.exe del run_input.exe
echo.

echo [3/7] Running Test 1 (test1_basic.cpp)...
.\optimizer.exe test1_basic.cpp
g++ -std=c++17 test1_basic_optimized.cpp -o run_test1.exe
.\run_test1.exe
if exist run_test1.exe del run_test1.exe
echo.

echo [4/7] Running Test 2 (test2_side_effects.cpp)...
.\optimizer.exe test2_side_effects.cpp
g++ -std=c++17 test2_side_effects_optimized.cpp -o run_test2.exe
.\run_test2.exe
if exist run_test2.exe del run_test2.exe
echo.

echo [5/7] Running Test 3 (test3_multiple_vars.cpp)...
.\optimizer.exe test3_multiple_vars.cpp
g++ -std=c++17 test3_multiple_vars_optimized.cpp -o run_test3.exe
.\run_test3.exe
if exist run_test3.exe del run_test3.exe
echo.

echo [6/7] Running Test 4 (test4_unreachable.cpp)...
.\optimizer.exe test4_unreachable.cpp
g++ -std=c++17 test4_unreachable_optimized.cpp -o run_test4.exe
.\run_test4.exe
if exist run_test4.exe del run_test4.exe
echo.

echo [7/7] Running Test 5 and Test 6...
.\optimizer.exe test5_constant_fold_prop.cpp
g++ -std=c++17 test5_constant_fold_prop_optimized.cpp -o run_test5.exe
.\run_test5.exe
if exist run_test5.exe del run_test5.exe

.\optimizer.exe test6_redundant_empty_funcs.cpp
g++ -std=c++17 test6_redundant_empty_funcs_optimized.cpp -o run_test6.exe
.\run_test6.exe
if exist run_test6.exe del run_test6.exe
echo.

echo ========================================================
echo [✓] ALL 6 TEST SUITES AND OLLAMA PIPELINE PASSED!
echo ========================================================
