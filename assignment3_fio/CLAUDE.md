# fio의 meta 방식을 이용하여 데이터 무결성 check simulator

fio의 데이터 무결성 check 중에는 meta 방법이 있는데 해당 방법을 실제 디스크 테스트용으로 구현한다.

## Building and Running
```bash
# make
make 

# Run the program
./fio_simulator
```

## Basic Knowledge

fio의 meta 방식은 메모리 블록을 write할 때 데이터 앞 부분을 `verify_header`로 두고 해당 구조체에 `lba`, `time_stamp`, `checksum`, `offset` 값을 저장한다.
`lba`는 fio가 읽을 때 부여하는 논리적 block 숫자로 sector라고도 불린다. OS에서는 1 LBA를 512 byte로 보고 해당 블록을 기준으로 read, write한다. 이 테스트에서는 512 byte씩 데이터를 read, write한다.

`timestamp`에는 현재 시간이 들어가고, `checksum`에서는 `crc32` checksum 알고리즘이 들어간다.

 `#include <smmintrin.h>`에 있는 intel의 하드웨어 crc 계산을 이용하였다.

<!-- - `crc32`에서 쓰이는 lookup table
```C
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
    0xBCD60DC4, 0xCAD13D52, 0x5C6985ED, 0x2B6E957B, Dos 0xBC67C50D, 0xCF60F59B,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7,
    0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
    0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9, 0x5005713C, 0x270241AA,
    0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
    0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
    0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683, 0xE3630B12, 0x94643B84,
    0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB,
    0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
    0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E,
    0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55,
    0x316E8EEF, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
    0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28,
    0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F,
    0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
    0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242,
    0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69,
    0x616BFFD3, 0x166CCF45, 0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
    0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC,
    0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693,
    0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
    0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};
``` -->

write 시에는 메모리에 쓸 데이터를 `crc32` checksum 알고리즘을 이용하여 계산하고 `verify_header`에 넣어준다.

`offset`은 해당 memory 위치가 0번째 주소에서 얼마나 떨어져 있는지를 나타낸다. 예를 들어 0번은 첫번째 memory이고 그 다음 memory에서는 512이라는 `offset`이 있다.

read를 할 때는 random으로 읽거나 순차적으로 읽어올 수 있다. read시 `lba`를 확인하여 `verify_header`에 쓰여진 것과 현재 읽을 `lba`가 서로 다른지 같은지 확인해야 한다. 그리고 `verify_header`를 읽고 checksum 값을 가져온다. 그리고 payload를 읽어와서 checksum 계산을 하고 값을 비교한다. 그리고 만약 다르다면 `time_stamp`와 `lba`를 출력해서 어디가 문제인지를 나타낸다.

### Core Design Pattern

arg parsing을 통해 verify를 진행하게 된다. verify에서 --test를 넣으면 read, write를 테스트하게 된다.

read, write 테스트는 `/dev/sdb` 의 파티션 안 된 실제 디스크에서 쓰게 된다.

memory block안에 `verify_header`을 두고 메모리 블록의 데이터는 이 header 다음에 들어간다. 그래서 data 시작 위치를 알아야 한다.
memory block마다 구조체의 크기 `offset`만큼을 더해서 시작 위치에 더해준 것을 시작 위치로 이용한다.

데이터 read를 하면서 random 모드, 순차 모드를 따로 둔다. 코드 실행 시 `--read --seq`, `--read --random`, 이렇게 주게 되고 `--seq`이라면 sequential하게 read를 하는 것이고, `--random`이라면 random하게 read하는 것이다. `--write`를 하면 write를 하게 된다. 그리고 `--corruption`을 주면 4가지 모두에 대한 에러를 넣게 된다.
random test일 경우 LBA를 random으로 뽑아내서 확인한다. sequential test일 경우 LBA를 0번부터 확인한다.

만약 read 시에 `verify_header`를 보고 write된 것인지 안된 것인지 판단하여 `verfiy_header` 데이터가 있다면 read test를 하고 아니라면 test를 안하도록 해야한다.

실행하면서 데이터에 결함이 있다면 log에 표시를 하면서 끝까지 진행한다.

각 sector마다 crc 값이 다르게 저장된다.

현재 속도 개선을 위해 병렬성을 추가하였다.

### key operation 
- memory block size는 512이다.

**simulation write**(`sim_write`):

**simulation read**(`sim_read`):

**calculation crc32 checksum**(`crc32_checksum`):

### Common Pitfalls
메모리를 시뮬레이션 상에서 만들어서 이용하는 것이므로 같은 메모리 안에서 `verify_header`와 payload가 들어갈 영역을 `offset`을 잘 두어서 서로의 영역을 침범하지 않도록 한다. 