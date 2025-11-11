#!/bin/bash

echo "=== FIO Simulator Debug Test ==="
echo ""

# 디버그 심볼과 함께 컴파일
echo "Compiling with debug symbols..."
gcc -g -o fio_simulator_debug fio_simulator.c -Wall -Wextra

if [ ! -f fio_simulator_debug ]; then
    echo "Compilation failed"
    exit 1
fi

echo "Running with gdb..."
echo "Commands you can use in gdb:"
echo "  - run --test seq     : 프로그램 실행"
echo "  - bt                 : backtrace (crash 위치 확인)"
echo "  - p variable_name    : 변수 값 확인"
echo "  - continue           : 계속 실행"
echo "  - quit               : 종료"
echo ""

gdb ./fio_simulator_debug
