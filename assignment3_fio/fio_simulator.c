#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <threads.h>
#include <smmintrin.h>
#include <stdatomic.h>
#include <pthread.h>

#define SECTOR_SIZE 512LLU
#define MONITOR_INTERVAL 10.0  // 2초 interval
#define IO_BLOCK_SIZE (131072LLU)  // 4KB, 12KB(12288), 32KB(32768), 128KB(131072), 256KB(262144) 중 선택 가능
#define ALIGNMENT 4096
#define SECTORS_PER_BLOCK (IO_BLOCK_SIZE / SECTOR_SIZE)
#define CHUNK 1000

// device 정보를 읽고 업데이트
uint64_t NUM_SECTOR = 0;
uint64_t NUM_BLOCKS = 0; 
uint64_t PROGRESS_PERCENT = 0;
uint64_t TOTAL_SIZE = 0;

// verify_header 구조체
typedef struct {
    uint64_t lba;          // Logical Block Address
    uint64_t timestamp;    // 시간
    uint32_t checksum;     // CRC32 체크섬
    uint64_t offset;       // 메모리 오프셋
} verify_header;

// 모니터링 스레드용 구조체
typedef struct {
    atomic_uint_fast64_t *completed_bytes;  // 완료된 바이트 수
    atomic_bool *stop_flag;                 // 종료 플래그
    const char *operation_name;             // 작업 이름 (WRITE/READ)
} monitor_context;

// 디바이스 파일 디스크립터
int device_fd = -1;
const char* device_path = "/dev/sdb";  // 블록 디바이스 경로
uint64_t device_size = 0; 

// CRC32 checksum 계산 함수
uint32_t crc32_checksum(const void *data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    const uint64_t *buf = (const uint64_t*)data;

    size_t i = 0;
    for(; i < length/8; i++)
    {
        crc = _mm_crc32_u64(crc, buf[i]);
    }
    // i가 수행되고 남아있는 것들에 대해서 수행
    const uint8_t *remaining = (const uint8_t*)(buf+i);
    for (i = 0; i< length % 8; i++)
    {
        crc = _mm_crc32_u8(crc, remaining[i]);
    }

    return ~crc;
}

void* monitor_thread(void* arg) {
    monitor_context *ctx = (monitor_context*)arg;
    struct timespec interval_start;
    clock_gettime(CLOCK_MONOTONIC, &interval_start);

    uint64_t last_bytes = 0;

    while (!atomic_load(ctx->stop_flag)) {
        struct timespec sleep_time = {
            .tv_sec = (time_t)MONITOR_INTERVAL,
            .tv_nsec = (long)((MONITOR_INTERVAL - (time_t)MONITOR_INTERVAL) * 1000000000)
        };
        nanosleep(&sleep_time, NULL);

        // 현재 시간과 완료된 바이트 읽기
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        uint64_t current_bytes = atomic_load(ctx->completed_bytes);

        // interval 동안 처리된 바이트
        uint64_t interval_bytes = current_bytes - last_bytes;

        // throughput 계산 (MB/s)
        double throughput = (interval_bytes / (1024.0 * 1024.0)) / MONITOR_INTERVAL;

        // 누적 처리량 (MB)
        double total_mb = current_bytes / (1024.0 * 1024.0);

        // 진행률 계산
        int progress = (int)((current_bytes * 100) / TOTAL_SIZE);
        if (progress > 100) progress = 100;

        // 출력
        printf("[%s Interval %.1fs] Throughput: %.2f MB/s | Total: %.2f MB | Progress: %d%%\n",
               ctx->operation_name, MONITOR_INTERVAL, throughput, total_mb, progress);

        last_bytes = current_bytes;
    }

    return NULL;
}


void sim_write_block(uint64_t start_lba, const unsigned char *data, size_t data_size, uint64_t timestamp) {
    // 시작 LBA에 해당하는 디바이스 오프셋 계산
    uint64_t offset = start_lba * SECTOR_SIZE;
    if (offset + IO_BLOCK_SIZE > device_size) {
        printf("Error: Block starting at LBA %lu exceeds device bounds\n", start_lba);
        return;
    }

    static thread_local unsigned char *block = NULL;
    if (block == NULL) {
        if (posix_memalign((void**)&block, ALIGNMENT, IO_BLOCK_SIZE) != 0) {
            perror("posix_memalign");
            return;
        }
    }

    // 각 섹터마다 verify_header 설정
    for (uint64_t i = 0; i < SECTORS_PER_BLOCK; i++) {
        uint64_t lba = start_lba + i;
        uint64_t sector_offset_in_block = i * SECTOR_SIZE;
        unsigned char *sector = block + sector_offset_in_block;

        // verify_header 설정
        verify_header *header = (verify_header *)sector;
        header->lba = lba;
        header->timestamp = timestamp;
        header->offset = lba * SECTOR_SIZE;

        // 데이터 영역 시작 위치
        unsigned char *payload = sector + sizeof(verify_header);
        size_t payload_size = SECTOR_SIZE - sizeof(verify_header);

        // 데이터 복사
        size_t data_offset = i * SECTOR_SIZE;
        size_t copy_size = payload_size;
        if (data_offset + copy_size > data_size) {
            copy_size = (data_size > data_offset) ? (data_size - data_offset) : 0;
        }

        if (copy_size > 0) {
            memcpy(payload, data + data_offset, copy_size);
        }

        // 남은 공간은 0으로 채움
        if (copy_size < payload_size) {
            memset(payload + copy_size, 0, payload_size - copy_size);
        }

        // checksum 계산 (payload에 대해서만)
        header->checksum = crc32_checksum(payload, payload_size);
    }

    // 디바이스에 블록 전체 쓰기
    ssize_t written = pwrite(device_fd, block, IO_BLOCK_SIZE, offset);
    if (written != (ssize_t)IO_BLOCK_SIZE) {
        printf("Error: Failed to write block at LBA %lu (written %ld bytes)\n", start_lba, written);
        perror("pwrite");
        return;
    }

}

int sim_read_block(uint64_t start_lba) {
    // 시작 LBA에 해당하는 디바이스 오프셋 계산
    uint64_t offset = start_lba * SECTOR_SIZE;

    if (offset + IO_BLOCK_SIZE > device_size) {
        printf("Error: Block starting at LBA %lu exceeds device bounds\n", start_lba);
        return -1;
    }

    // Thread-local 버퍼 사용 (메모리 할당/해제 오버헤드 제거)
    static thread_local unsigned char *block = NULL;
    if (block == NULL) {
        if (posix_memalign((void**)&block, ALIGNMENT, IO_BLOCK_SIZE) != 0) {
            perror("posix_memalign");
            return -1;
        }
    }

    // 디바이스에서 블록 전체 읽기
    ssize_t bytes_read = pread(device_fd, block, IO_BLOCK_SIZE, offset);
    if (bytes_read != (ssize_t)IO_BLOCK_SIZE) {
        printf("Error: Failed to read block at LBA %lu (read %ld bytes)\n", start_lba, bytes_read);
        perror("pread");
        return -1;
    }

    int errors = 0;

    // 각 섹터마다 검증
    for (uint64_t i = 0; i < SECTORS_PER_BLOCK; i++) {
        uint64_t lba = start_lba + i;
        uint64_t sector_offset_in_block = i * SECTOR_SIZE;
        unsigned char *sector = block + sector_offset_in_block;

        // verify_header 읽기
        verify_header *header = (verify_header *)sector;

        // LBA 검증
        if (header->lba != lba) {
            printf("[ERROR] LBA mismatch at LBA=%lu: Expected=%lu, Got=%lu, Timestamp=%lu\n",
                   lba, lba, header->lba, header->timestamp);
            errors++;
            continue;
        }

        // Offset 검증
        uint64_t expected_offset = lba * SECTOR_SIZE;
        if (header->offset != expected_offset) {
            printf("[ERROR] Offset mismatch at LBA=%lu: Expected=%lu, Got=%lu, Timestamp=%lu\n",
                   lba, expected_offset, header->offset, header->timestamp);
            errors++;
            continue;
        }

        // payload 읽기 및 checksum 계산
        unsigned char *payload = sector + sizeof(verify_header);
        size_t payload_size = SECTOR_SIZE - sizeof(verify_header);
        uint32_t calculated_checksum = crc32_checksum(payload, payload_size);

        // checksum 검증
        if (calculated_checksum != header->checksum) {
            printf("[ERROR] Checksum mismatch at LBA=%lu: Expected=0x%08X, Got=0x%08X, Timestamp=%lu\n",
                   lba, header->checksum, calculated_checksum, header->timestamp);
            errors++;
        }
    }

    // Thread-local 버퍼는 재사용되므로 free하지 않음
    return errors;  // 에러 섹터 개수 반환 (0 이상)
}


void test_sequential(void) {
    printf("\n=== Sequential Test ===\n");
    printf("Reading all blocks sequentially...\n\n");

    // 모니터링용 atomic 변수
    atomic_uint_fast64_t completed_bytes = 0;
    atomic_bool stop_flag = false;
    atomic_int errors = 0;
    atomic_int read_failures = 0;

    // 모니터링 컨텍스트 설정
    monitor_context ctx = {
        .completed_bytes = &completed_bytes,
        .stop_flag = &stop_flag,
        .operation_name = "READ"
    };

    // 모니터링 스레드 시작
    pthread_t monitor_tid;
    pthread_create(&monitor_tid, NULL, monitor_thread, &ctx);

    #pragma omp parallel for schedule(dynamic, CHUNK)
    for (uint64_t block_idx = 0; block_idx < NUM_BLOCKS; block_idx++) {
        uint64_t start_lba = block_idx * SECTORS_PER_BLOCK;
        int block_errors = sim_read_block(start_lba);

        if (block_errors > 0) {
            // 섹터 에러 개수 누적
            atomic_fetch_add(&errors, block_errors);
        } else if (block_errors < 0) {
            // 읽기 실패 (I/O 에러 등)
            atomic_fetch_add(&read_failures, 1);
        }

        // 완료된 바이트 수 업데이트
        atomic_fetch_add(&completed_bytes, IO_BLOCK_SIZE);
    }

    // 모니터링 스레드 종료
    atomic_store(&stop_flag, true);
    pthread_join(monitor_tid, NULL);

    int total_errors = atomic_load(&errors);
    int total_failures = atomic_load(&read_failures);
    printf("\nSequential Test Complete:\n");
    printf("  Sector errors: %d\n", total_errors);
    printf("  Read failures: %d blocks\n", total_failures);
}

void test_random(void) {
    printf("\n=== Random Test ===\n");
    printf("Reading blocks in random order...\n\n");

    // 모니터링용 atomic 변수
    atomic_uint_fast64_t completed_bytes = 0;
    atomic_bool stop_flag = false;
    atomic_int errors = 0;
    atomic_int read_failures = 0;

    // 모니터링 컨텍스트 설정
    monitor_context ctx = {
        .completed_bytes = &completed_bytes,
        .stop_flag = &stop_flag,
        .operation_name = "READ"
    };

    // 모니터링 스레드 시작
    pthread_t monitor_tid;
    pthread_create(&monitor_tid, NULL, monitor_thread, &ctx);

    #pragma omp parallel for schedule(dynamic, CHUNK)
    for (unsigned long long i = 0; i < NUM_BLOCKS; i++) {
        // Thread-safe random number generation
        static thread_local unsigned int seed = 0;
        if (seed == 0) {
            seed = (unsigned int)(time(NULL) ^ (uintptr_t)&seed);
        }

        uint64_t block_idx = rand_r(&seed) % NUM_BLOCKS;
        uint64_t start_lba = block_idx * SECTORS_PER_BLOCK;
        int block_errors = sim_read_block(start_lba);

        if (block_errors > 0) {
            // 섹터 에러 개수 누적
            atomic_fetch_add(&errors, block_errors);
        } else if (block_errors < 0) {
            // 읽기 실패 (I/O 에러 등)
            atomic_fetch_add(&read_failures, 1);
        }

        // 완료된 바이트 수 업데이트
        atomic_fetch_add(&completed_bytes, IO_BLOCK_SIZE);
    }

    // 모니터링 스레드 종료
    atomic_store(&stop_flag, true);
    pthread_join(monitor_tid, NULL);

    int total_errors = atomic_load(&errors);
    int total_failures = atomic_load(&read_failures);
    printf("\nRandom Test Complete:\n");
    printf("  Sector errors: %d\n", total_errors);
    printf("  Read failures: %d blocks\n", total_failures);
}

// 테스트 데이터로 메모리 초기화
void initialize_memory(void) {
    printf("Initializing memory with test data...\n\n");

    // 모니터링용 atomic 변수
    atomic_uint_fast64_t completed_bytes = 0;
    atomic_bool stop_flag = false;

    // 모니터링 컨텍스트 설정
    monitor_context ctx = {
        .completed_bytes = &completed_bytes,
        .stop_flag = &stop_flag,
        .operation_name = "WRITE"
    };

    // 모니터링 스레드 시작
    pthread_t monitor_tid;
    pthread_create(&monitor_tid, NULL, monitor_thread, &ctx);

    #pragma omp parallel for schedule(dynamic, CHUNK)
    for (uint64_t block_idx = 0; block_idx < NUM_BLOCKS; block_idx++) {
        static thread_local uint64_t thread_timestamp = 0;

        if (block_idx % CHUNK == 0)
        {
            thread_timestamp = (uint64_t)time(NULL);
        }

        uint64_t start_lba = block_idx * SECTORS_PER_BLOCK;

        // 블록 크기만큼 테스트 데이터 생성
        unsigned char data[IO_BLOCK_SIZE];
        for (size_t i = 0; i < IO_BLOCK_SIZE; i++) {
            // 각 섹터마다 다른 패턴 생성
            uint64_t sector_in_block = i / SECTOR_SIZE;
            uint64_t lba = start_lba + sector_in_block;
            data[i] = (unsigned char)((lba * 256 + i) % 256);
        }

        sim_write_block(start_lba, data, IO_BLOCK_SIZE, thread_timestamp);

        // 완료된 바이트 수 업데이트
        atomic_fetch_add(&completed_bytes, IO_BLOCK_SIZE);
    }

    // 모니터링 스레드 종료
    atomic_store(&stop_flag, true);
    pthread_join(monitor_tid, NULL);

    printf("\nMemory initialization complete\n");
    printf("Read start\n");
}

void corruption(int corruption_type)
{
    unsigned char *block = NULL;
    if (posix_memalign((void**)&block, SECTOR_SIZE, SECTOR_SIZE) != 0) {
        perror("posix_memalign");
        return;
    }

    ssize_t ret;
    switch(corruption_type){
        case 1: {  // checksum mismatch
            ret = pread(device_fd, block, SECTOR_SIZE, 5*SECTOR_SIZE);
            if (ret != SECTOR_SIZE) {
                perror("pread failed in corruption case 1");
                break;
            }
            block[sizeof(verify_header)] = 'A';
            ret = pwrite(device_fd, block, SECTOR_SIZE, 5*SECTOR_SIZE);
            if (ret != SECTOR_SIZE) {
                perror("pwrite failed in corruption case 1");
            }
            break;
        }
        case 2: {  // lba mismatch
            ret = pread(device_fd, block, SECTOR_SIZE, 3*SECTOR_SIZE);
            if (ret != SECTOR_SIZE) {
                perror("pread failed in corruption case 2");
                break;
            }
            verify_header* header = (verify_header*)block;
            header->lba = 4;
            ret = pwrite(device_fd, block, SECTOR_SIZE, 3*SECTOR_SIZE);
            if (ret != SECTOR_SIZE) {
                perror("pwrite failed in corruption case 2");
            }
            break;
        }
        case 3: {  // offset mismatch
            ret = pread(device_fd, block, SECTOR_SIZE, 1*SECTOR_SIZE);
            if (ret != SECTOR_SIZE) {
                perror("pread failed in corruption case 3");
                break;
            }
            verify_header* header = (verify_header*)block;
            header->offset = 8192;
            ret = pwrite(device_fd, block, SECTOR_SIZE, 1*SECTOR_SIZE);
            if (ret != SECTOR_SIZE) {
                perror("pwrite failed in corruption case 3");
            }
            break;
        }
        case 4: {  // lba and offset mismatch
            ret = pread(device_fd, block, SECTOR_SIZE, 4*SECTOR_SIZE);
            if (ret != SECTOR_SIZE) {
                perror("pread failed in corruption case 4");
                break;
            }
            verify_header* header = (verify_header*)block;
            header->offset = 0;
            header->lba = 2;
            ret = pwrite(device_fd, block, SECTOR_SIZE, 4*SECTOR_SIZE);
            if (ret != SECTOR_SIZE) {
                perror("pwrite failed in corruption case 4");
            }
            break;
        }
        default:
            printf("Unknown corruption type: %d\n", corruption_type);
            break;
    }

    free(block);
}

// CRC 값 출력 
void print_sample_checksums(void) {
    printf("\n=== CRC Checksum Info ===\n");

    uint64_t lba = 0;
    uint64_t offset = lba * SECTOR_SIZE;
    unsigned char *block = NULL;

    if (posix_memalign((void**)&block, SECTOR_SIZE, SECTOR_SIZE) != 0) {
        printf("Failed to allocate memory for checksum verification\n");
        return;
    }

    ssize_t bytes_read = pread(device_fd, block, SECTOR_SIZE, offset);
    if (bytes_read == SECTOR_SIZE) {
        verify_header *header = (verify_header *)block;
        printf("CRC32 Checksum: 0x%08X\n", header->checksum);
        printf("(This value should be consistent across all blocks)\n");
    } else {
        printf("Failed to read block for checksum verification\n");
    }

    free(block);
    printf("\n");
}

void print_usage(const char *prog_name) {
    printf("Usage: %s --test <mode>\n", prog_name);
    printf("Modes:\n");
    printf("  seq     : Sequential read test\n");
    printf("  random  : Random read test\n");
}

int main(int argc, char *argv[]) {
    printf("FIO Meta Verification Simulator\n");
    printf("================================\n");
    printf("Sector Size: %llu bytes\n", SECTOR_SIZE);
    printf("I/O Block Size: %llu bytes (%llu sectors per block)\n", IO_BLOCK_SIZE, SECTORS_PER_BLOCK);
    printf("Header Size: %lu bytes\n", sizeof(verify_header));
    printf("Payload Size per Sector: %llu bytes\n\n", SECTOR_SIZE - sizeof(verify_header));

    // 인자 파싱
    if (argc != 3 || strcmp(argv[1], "--test") != 0) {
        print_usage(argv[0]);
        return 1;
    }

    // 블록 디바이스 열기
    printf("Opening block device: %s\n", device_path);
    device_fd = open(device_path, O_RDWR | O_DIRECT);
    if (device_fd == -1) {
        perror("Failed to open block device");
        printf("Note: You may need to run with sudo privileges\n");
        return 1;
    }

    // 디바이스 크기 확인
    if (ioctl(device_fd, BLKGETSIZE64, &device_size) == -1) {
        perror("Failed to get device size");
        close(device_fd);
        return 1;
    }

    printf("Device size: %llu bytes (%.2f GB)\n", (unsigned long long)device_size, device_size / (1024.0 * 1024.0 * 1024.0));

    // 디바이스 크기를 기반으로 동적 계산
    NUM_SECTOR = device_size / SECTOR_SIZE;
    NUM_BLOCKS = device_size / IO_BLOCK_SIZE;
    PROGRESS_PERCENT = NUM_SECTOR / 100;  // 1%마다 출력하도록 변경

    TOTAL_SIZE = SECTOR_SIZE * NUM_SECTOR;

    printf("Number of Sectors: %lu\n", NUM_SECTOR);
    printf("Number of I/O Blocks: %lu\n", NUM_BLOCKS);
    printf("Total Size to Test: %lu bytes (%.2f GB)\n", TOTAL_SIZE, TOTAL_SIZE / (1024.0 * 1024.0 * 1024.0));
    printf("Block device opened successfully!\n\n");

    
    //struct timespec start, end_time;
    //clock_gettime(CLOCK_MONOTONIC, &start); 
    initialize_memory();
    //clock_gettime(CLOCK_MONOTONIC, &end_time);

    // double write_time = (end_time.tv_sec - start.tv_sec) +
    //                     (end_time.tv_nsec - start.tv_nsec) / 1000000000.0;
    // double write_throughput = (TOTAL_SIZE / (1024.0 * 1024.0)) / write_time; // MB/s
    // double write_iops = NUM_BLOCKS / write_time;  
    
    // printf("\n === data corruption === \n");

    // corruption(1);
    // corruption(2);
    // corruption(3);
    // corruption(4);


    // 테스트 모드 실행
    if (strcmp(argv[2], "seq") == 0) {
        //clock_gettime(CLOCK_MONOTONIC, &start);
        test_sequential();
    } else if (strcmp(argv[2], "random") == 0) {
        //clock_gettime(CLOCK_MONOTONIC, &start);
        test_random();
    } else {
        printf("Error: Unknown test mode '%s'\n", argv[2]);
        print_usage(argv[0]);
        close(device_fd);
        return 1;
    }

    //clock_gettime(CLOCK_MONOTONIC, &end_time);
    // double read_time = (end_time.tv_sec - start.tv_sec) +
    //                    (end_time.tv_nsec - start.tv_nsec) / 1000000000.0;
    // double read_throughput = (TOTAL_SIZE / (1024.0 * 1024.0)) / read_time; // MB/s
    // double read_iops = NUM_BLOCKS / read_time;  // I/O operations per second (block 단위)

    // printf("\n=== I/O Performance Metrics ===\n");
    // printf("[WRITE]\n");
    // printf("  Time: %.2f seconds\n", write_time);
    // printf("  Throughput: %.2f MB/s\n", write_throughput);
    // printf("  IOPS: %.0f ops/sec\n\n", write_iops);

    // printf("[READ]\n");
    // printf("  Time: %.2f seconds\n", read_time);
    // printf("  Throughput: %.2f MB/s\n", read_throughput);
    // printf("  IOPS: %.0f ops/sec\n", read_iops);

    // 샘플 블록들의 CRC 값 출력
    print_sample_checksums();

    // 정리
    printf("\nCleaning up...\n");
    close(device_fd);
    printf("Done.\n");

    return 0;
}