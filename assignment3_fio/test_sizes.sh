#!/bin/bash

# 테스트할 크기들 (NUM_BLOCKS 값)
# 10GB  = 2621440 blocks
# 50GB  = 13107200 blocks
# 100GB = 26214400 blocks
# 500GB = 131072000 blocks (현재)
# 1TB   = 262144000 blocks

TEST_SIZES=(
    "2621440:10GB"
    "13107200:50GB"
    "26214400:100GB"
    "131072000:500GB"
    "262144000:1TB"
)

echo "=== FIO Simulator Size Test ==="
echo ""

for size_info in "${TEST_SIZES[@]}"; do
    NUM_BLOCKS=$(echo $size_info | cut -d: -f1)
    SIZE_NAME=$(echo $size_info | cut -d: -f2)

    echo "----------------------------------------"
    echo "Testing with $SIZE_NAME (NUM_BLOCKS=$NUM_BLOCKS)"
    echo "----------------------------------------"

    # NUM_BLOCKS 값 변경
    sed -i "s/#define NUM_BLOCKS.*/#define NUM_BLOCKS ${NUM_BLOCKS}ULL/" fio_simulator.c

    # TEN_PERCENT 값도 업데이트
    TEN_PERCENT=$((NUM_BLOCKS / 10))
    sed -i "s/#define TEN_PERCENT.*/#define TEN_PERCENT ${TEN_PERCENT}ULL/" fio_simulator.c

    # 컴파일
    echo "Compiling..."
    if ! gcc -o fio_simulator fio_simulator.c -Wall -Wextra; then
        echo "ERROR: Compilation failed for $SIZE_NAME"
        continue
    fi

    # Sequential 테스트 실행
    echo "Running sequential test..."
    if timeout 300 ./fio_simulator --test seq 2>&1 | tee test_${SIZE_NAME}_seq.log; then
        echo "SUCCESS: $SIZE_NAME sequential test completed"
    else
        EXIT_CODE=$?
        echo "FAILED: $SIZE_NAME sequential test failed with code $EXIT_CODE"
        if [ $EXIT_CODE -eq 139 ]; then
            echo "  -> SEGMENTATION FAULT (core dump)"
        fi
        dmesg | tail -5
        break
    fi

    echo ""

    # 정리
    rm -f fio_simulator.dat
done

echo ""
echo "=== Test Complete ==="
