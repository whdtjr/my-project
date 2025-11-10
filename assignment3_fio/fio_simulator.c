#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
//#include <errno.h>

#define BLOCK_SIZE 4096ULL
#define NUM_BLOCKS 131072000ULL
#define TEN_PERCENT 13107200ULL
#define TOTAL_SIZE (BLOCK_SIZE * NUM_BLOCKS)

uint64_t WRITE_COUNT = 0;
uint64_t READ_COUNT = 0;

// CRC32 lookup table
static const uint32_t crc32_table[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F,
    0xE963A535, 0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
    0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2,
    0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9,
    0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
    0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
    0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423,
    0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
    0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D, 0x76DC4190, 0x01DB7106,
    0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D,
    0x91646C97, 0xE6635C01, 0x6B6B30C4, 0x1C6C0052, 0x856551E8, 0xF262617E,
    0x6C0694DD, 0x1B01A44B, 0x8208F5F1, 0xF50FC567, 0x65B0D8D6, 0x12B7E840,
    0x8BBEC9FA, 0xFDBDF96C, 0x63B8A7D5, 0x14BE9743, 0x8DB71010, 0xFAB02086,
    0x63D2D6A3, 0x14D5E635, 0x8DDCD78F, 0xFABD8719, 0x5B6E10A8, 0x2C69203E,
    0xB5605184, 0xC2676112, 0x5C03F4B1, 0x2B04C427, 0xB20D959D, 0xC50AB50B,
    0x55B5A89A, 0x22B2980C, 0xBBBBD9BD, 0xCCBCF92B, 0x52D86CE8, 0x25DF5C7E,
    0xBCD60DC4, 0xCAD13D52, 0x5C6985ED, 0x2B6E957B, 0xB2641A5C, 0xC5635ABF,
    0x5C0A4542, 0x2B0D75D4, 0xB3BC6066, 0xC4BB50F0, 0x5AD2C553, 0x2DD5F5C5,
    0xB4DCA47F, 0xC3DB74E9, 0x53647A78, 0x24634AEE, 0xBD6A1B54, 0xCA6D2BC2,
    0x5404BE61, 0x23038EF7, 0xBA0ADF4D, 0xCD0DEFDB, 0x5DE2F04C, 0x2AE5C0DA,
    0xB3EC9160, 0xC4EBA1F6, 0x5A8B3455, 0x2D8C04C3, 0xB4855579, 0xC38265EF,
    0x53397B7E, 0x243E4BE8, 0xBD371A52, 0xCA302AC4, 0x5459BF67, 0x235E8FF1,
    0xBA57DE4B, 0xCD50EEDD, 0x5DF9E34C, 0x2AFED3DA, 0xB3F78260, 0xC4F0B2F6,
    0x5A992755, 0x2D9E17C3, 0xB4974679, 0xC39076EF, 0x532F6B9E, 0x24285B08,
    0xBD210AB2, 0xCA263A24, 0x544FAF87, 0x23489F11, 0xBA41CEAB, 0xCD46FE3D,
    0x5DF9E1AC, 0x2AFED13A, 0xB3F78080, 0xC4F0B016, 0x5A9925B5, 0x2D9E1523,
    0xB4974499, 0xC390740F, 0x532F699E, 0x24285908, 0xBD2108B2, 0xCA263824,
    0x544FAD87, 0x23489D11, 0xBA41CCAB, 0xCD46FC3D, 0x0D4326D9, 0x7A44164F,
    0xE34D47F5, 0x944A7763, 0x0A23E2C0, 0x7D24D256, 0xE42D83EC, 0x932AB37A,
    0x0395AEEB, 0x74929E7D, 0xED9BCFC7, 0x9A9CFF51, 0x04F56AF2, 0x73F25A64,
    0xEAFB0BDE, 0x9DFC3B48, 0x0D4324D9, 0x7A44144F, 0xE34D45F5, 0x944A7563,
    0x0A23E0C0, 0x7D24D056, 0xE42D81EC, 0x932AB17A, 0x0395ACEB, 0x74929C7D,
    0xED9BCDC7, 0x9A9CFD51, 0x04F568F2, 0x73F25864, 0xEAFB09DE, 0x9DFC3948
};

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
    const unsigned char *buf = (const unsigned char *)data;

    for (size_t i = 0; i < length; i++) {
        uint8_t index = (crc ^ buf[i]) & 0xFF;
        crc = (crc >> 8) ^ crc32_table[index];
    }

    return ~crc;
}

// 시뮬레이션 write 함수
void sim_write(uint64_t lba, const unsigned char *data, size_t data_size) {
    // LBA에 해당하는 메모리 위치 계산
    uint64_t offset = lba * BLOCK_SIZE;

    if (offset + BLOCK_SIZE > TOTAL_SIZE) {
        printf("Error: LBA %lu exceeds memory bounds\n", lba);
        return;
    }

    // verify_header 설정
    verify_header *header = (verify_header *)(memory + offset);
    header->lba = lba;
    header->timestamp = (uint64_t)time(NULL);
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

    WRITE_COUNT++;
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
    READ_COUNT++;
    return 0;
}

// Sequential test
void test_sequential(void) {
    printf("\n=== Sequential Test ===\n");
    printf("Reading all blocks sequentially...\n\n");

    int errors = 0;
    for (uint64_t lba = 0; lba < NUM_BLOCKS; lba++) {
        if (sim_read(lba) != 0) {
            errors++;
        }
        if (READ_COUNT % TEN_PERCENT  == 0)
        {
            int precent = (lba / TEN_PERCENT)*10;
            printf("[READ Progress] %d%%\n", precent);
        }
    }

    printf("\nSequential Test Complete: %d errors found\n", errors);
}

// Random test
void test_random(void) {
    printf("\n=== Random Test ===\n");
    printf("Reading blocks in random order...\n\n");

    srand(time(NULL));
    int errors = 0;
    unsigned long long num_reads = NUM_BLOCKS; // Read twice as many times for thorough testing

    for (unsigned long long i = 0; i < num_reads; i++) {
        uint64_t lba = rand() % NUM_BLOCKS;
        if (sim_read(lba) != 0) {
            errors++;
        }
        if (READ_COUNT % TEN_PERCENT  == 0)
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

    for (uint64_t lba = 0; lba < NUM_BLOCKS; lba++) {
        unsigned char data[BLOCK_SIZE];
        for (size_t i = 0; i < BLOCK_SIZE; i++) {
            data[i] = (unsigned char)((lba * 256 + i) % 256);
        }
        sim_write(lba, data, BLOCK_SIZE);
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
    memory = (unsigned char*)mmap(NULL, TOTAL_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, memory_fd, 0);
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
    printf("write time : %f\n", write_cpu_time_used);
    printf("read time : %f\n", read_cpu_time_used);
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
