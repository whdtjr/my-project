#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
//#include <errno.h>
#include <threads.h>
#include <smmintrin.h>
#include <stdatomic.h>

#define BLOCK_SIZE 4096ULL
#define NUM_BLOCKS 131072000ULL
#define TEN_PERCENT 13107200ULL
#define TOTAL_SIZE (BLOCK_SIZE * NUM_BLOCKS)
#define CHUNK 1000



// verify_header 구조체
typedef struct {
    uint64_t lba;          // Logical Block Address
    uint64_t timestamp;    // 시간
    uint32_t checksum;     // CRC32 체크섬
    uint64_t offset;       // 메모리 오프셋
} verify_header;

// 전역 메모리 영역 (시뮬레이션용)
unsigned char* memory = NULL;
int memory_fd = -1;
const char* memory_file = "fio_simulator.dat";

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

// 시뮬레이션 write 함수
void sim_write(uint64_t lba, const unsigned char *data, size_t data_size, uint64_t timestamp) {
    // LBA에 해당하는 메모리 위치 계산
    uint64_t offset = lba * BLOCK_SIZE;
    if (offset + BLOCK_SIZE > TOTAL_SIZE) {
        printf("Error: LBA %lu exceeds memory bounds\n", lba);
        return;
    }

    // verify_header 설정
    verify_header *header = (verify_header *)(memory + offset);
    header->lba = lba;
    header->timestamp = timestamp;
    header->offset = offset;

    // 데이터 영역 시작 위치
    unsigned char *payload = memory + offset + sizeof(verify_header);
    size_t payload_size = BLOCK_SIZE - sizeof(verify_header);

    // 데이터 복사
    if (data_size > payload_size) {
        data_size = payload_size;
    }
    memcpy(payload, data, data_size);

    // 남은 공간은 0으로 채움
    if (data_size < payload_size) {
        memset(payload + data_size, 0, payload_size - data_size);
    }

    // checksum 계산 (payload에 대해서만)
    header->checksum = crc32_checksum(payload, payload_size);

    // printf("[WRITE] LBA=%lu, Offset=%lu, Timestamp=%lu, Checksum=0x%08X\n",
    //        header->lba, header->offset, header->timestamp, header->checksum);
}

// 시뮬레이션 read 및 verify 함수
int sim_read(uint64_t lba) {
    // LBA에 해당하는 메모리 위치 계산
    uint64_t offset = lba * BLOCK_SIZE;

    if (offset + BLOCK_SIZE > TOTAL_SIZE) {
        printf("Error: LBA %lu exceeds memory bounds\n", lba);
        return -1;
    }

    // verify_header 읽기
    verify_header *header = (verify_header *)(memory + offset);

    // LBA 검증
    if (header->lba != lba) {
        printf("[ERROR] LBA mismatch at LBA=%lu: Expected=%lu, Got=%lu, Timestamp=%lu\n",
               lba, lba, header->lba, header->timestamp);
        return -1;
    }

    // Offset 검증
    if (header->offset != offset) {
        printf("[ERROR] Offset mismatch at LBA=%lu: Expected=%lu, Got=%lu, Timestamp=%lu\n",
               lba, offset, header->offset, header->timestamp);
        return -1;
    }

    // payload 읽기 및 checksum 계산
    unsigned char *payload = memory + offset + sizeof(verify_header);
    size_t payload_size = BLOCK_SIZE - sizeof(verify_header);
    uint32_t calculated_checksum = crc32_checksum(payload, payload_size);

    // checksum 검증
    if (calculated_checksum != header->checksum) {
        printf("[ERROR] Checksum mismatch at LBA=%lu: Expected=0x%08X, Got=0x%08X, Timestamp=%lu\n",
               lba, header->checksum, calculated_checksum, header->timestamp);
        return -1;
    }

    // printf("[READ] LBA=%lu, Offset=%lu, Timestamp=%lu, Checksum=0x%08X - OK\n",
    //        header->lba, header->offset, header->timestamp, header->checksum);
    return 0;
}

// Sequential test
void test_sequential(void) {
    printf("\n=== Sequential Test ===\n");
    printf("Reading all blocks sequentially...\n\n");

    atomic_uint_fast64_t completed_count = 0;
    atomic_int errors = 0;

    #pragma omp parallel for schedule(dynamic, CHUNK)
    for (uint64_t lba = 0; lba < NUM_BLOCKS; lba++) {
        if (sim_read(lba) != 0) {
            atomic_fetch_add(&errors, 1);
        }
        uint64_t count = atomic_fetch_add(&completed_count, 1);

        if (count % TEN_PERCENT  == 0)
        {
            #pragma omp critical(sequential_progress)
            {
                int precent = (count / TEN_PERCENT)*10;
                printf("[READ Progress] %d%%\n", precent);
            }
        }
    }

    printf("\nSequential Test Complete: %d errors found\n", atomic_load(&errors));
}

// Random test 아직 병렬 적용 x
void test_random(void) {
    printf("\n=== Random Test ===\n");
    printf("Reading blocks in random order...\n\n");

    srand(time(NULL));
    int errors = 0;
    for (unsigned long long i = 0; i < NUM_BLOCKS; i++) {
        uint64_t lba = rand() % NUM_BLOCKS;
        if (sim_read(lba) != 0) {
            errors++;
        }
        if (i % TEN_PERCENT == 0)
        {
            int precent = (i / TEN_PERCENT)*10;
            printf("[READ Progress] %d%%\n", precent);
        }
    }

    printf("\nRandom Test Complete: %d errors found\n", errors);
}

// 테스트 데이터로 메모리 초기화
void initialize_memory(void) {
    printf("Initializing memory with test data...\n\n");
    #pragma omp parallel for schedule(dynamic, CHUNK)
    for (uint64_t lba = 0; lba < NUM_BLOCKS; lba++) {
        static thread_local uint64_t thread_timestamp = 0;

        if (lba % CHUNK == 0)
        {
            thread_timestamp = (uint64_t)time(NULL);
        }

        unsigned char data[BLOCK_SIZE];
        for (size_t i = 0; i < BLOCK_SIZE; i++) {
            data[i] = (unsigned char)((lba * 256 + i) % 256);
        }
        sim_write(lba, data, BLOCK_SIZE, thread_timestamp);
        if( lba % TEN_PERCENT == 0){
            #pragma omp critical(write_progress)
            {
                int precent = (lba / TEN_PERCENT)*10;
                printf(" [WRITE Progress] %d%%\n", precent);
            }
        }
    }

    printf("\nMemory initialization complete\n");
    printf("Read start\n");
}

void corruption(int corruption_type)
{

    switch(corruption_type){
        case 1: {
            *(memory+5*BLOCK_SIZE+sizeof(verify_header)) = 'A';
            break;
        } // checksum mismatch
        case 2: {
            verify_header* header = (verify_header*)(memory + 3*BLOCK_SIZE);
            header->lba = 4;
            break;
        } // lba mismatch
        case 3: {
            verify_header* header = (verify_header*)(memory + 1*BLOCK_SIZE);
            header->offset = 8192;
            break;
        }// offset mismatch
        case 4: {
            verify_header* header = (verify_header*)(memory + 4* BLOCK_SIZE);
            header->offset = 0;
            header->lba = 2;
        }
        default:
            break;
    }
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
    printf("Block Size: %llu bytes\n", BLOCK_SIZE);
    printf("Number of Blocks: %llu\n", NUM_BLOCKS);
    printf("Total Memory: %llu bytes (%.2f GB)\n", TOTAL_SIZE, TOTAL_SIZE / (1024.0 * 1024.0 * 1024.0));
    printf("Header Size: %lu bytes\n", sizeof(verify_header));
    printf("Payload Size: %llu bytes\n\n", BLOCK_SIZE - sizeof(verify_header));

    // 인자 파싱
    if (argc != 3 || strcmp(argv[1], "--test") != 0) {
        print_usage(argv[0]);
        return 1;
    }

    // 파일 생성 및 크기 설정
    printf("Creating memory-mapped file: %s\n", memory_file);
    memory_fd = open(memory_file, O_RDWR | O_CREAT, 0666);
    if (memory_fd == -1) {
        perror("Failed to create file");
        return 1;
    }

    // 파일 크기를 500GB로 설정
    if (ftruncate(memory_fd, TOTAL_SIZE) == -1) {
        perror("Failed to set file size");
        close(memory_fd);
        unlink(memory_file);
        return 1;
    }

    // mmap을 사용하여 메모리 매핑
    printf("Mapping %llu bytes to memory...\n", TOTAL_SIZE);
    memory = (unsigned char*)mmap(NULL, TOTAL_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, memory_fd, 0);
    if (memory == MAP_FAILED) {
        perror("Failed to mmap");
        close(memory_fd);
        unlink(memory_file);
        return 1;
    }

    printf("Memory mapping successful!\n\n");

    // 메모리 초기화
    clock_t start = clock(); // 1. 측정 시작 (CPU 틱 기록)
    initialize_memory();
    clock_t end_time = clock();

    double write_cpu_time_used = (double)(end_time - start) / CLOCKS_PER_SEC;
    
    // printf("\n === data corruption === \n");

    // corruption(1);
    // corruption(2);
    // corruption(3);
    // corruption(4);


    // 테스트 모드 실행
    if (strcmp(argv[2], "seq") == 0) {
        start = clock();
        test_sequential();
    } else if (strcmp(argv[2], "random") == 0) {
        start = clock();
        test_random();
    } else {
        printf("Error: Unknown test mode '%s'\n", argv[2]);
        print_usage(argv[0]);
        munmap(memory, TOTAL_SIZE);
        close(memory_fd);
        unlink(memory_file);
        return 1;
    }

    end_time = clock();
    double read_cpu_time_used = (double)(end_time - start) / CLOCKS_PER_SEC;

    printf(" \n=== I/O time check ===\n");
    printf("write time : %f sec\n", write_cpu_time_used);
    printf("read time : %f sec\n", read_cpu_time_used);
    // 정리
    printf("\nCleaning up...\n");
    if (munmap(memory, TOTAL_SIZE) == -1) {
        perror("Failed to munmap");
    }
    close(memory_fd);
    unlink(memory_file);
    printf("Done.\n");

    return 0;
}