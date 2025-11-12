#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>

#define SIZE 128
#define NULL_POINT 0xFFFF
// Gmem 모든 메모리는 여기서 할당 및 dealloc 된다.
uint8_t GMEM[SIZE];

// List manager
typedef struct {
    uint16_t head;        // 할당된 구역의 head
    uint16_t tail;        // 할당된 구역의 tail
    uint16_t free_head;   // free 구역의 head
    uint32_t free_count;  // 이용 가능한 메모리 개수
} LIST_MANAGER;

// manager 초기화 
void init_list(LIST_MANAGER* manager) {
    manager->head = NULL_POINT;       
    manager->tail = NULL_POINT;
    manager->free_head = 0;           
    manager->free_count = SIZE;

    //Gmem에서 index에는 값으로 next index로 값을 저장한다.
    for (uint16_t i = 0; i < SIZE; i++) {
        GMEM[i] = (i + 1) % SIZE;
    }

    printf("%d 메모리 초기화 완료\n", SIZE);
}


// 새롭게 생긴 head index를 return
uint16_t alloc_list(uint32_t count, LIST_MANAGER* manager) {
    if (count == 0) {
        printf("오류: 할당 개수가 0입니다 \n");
        return NULL_POINT;
    }

    if (count > manager->free_count) {
        printf("오류: 사용 가능한 노드가 부족합니다 (요청: %u, 사용 가능: %u)\n",
               count, manager->free_count);
        return NULL_POINT;
    }

    // alloc 가능한 head index를 따로 두어서 사용 가능한 memory를 추적
    uint16_t alloc_start = manager->free_head;
    uint16_t current = manager->free_head;
    uint16_t prev = NULL_POINT;

    // alloc할 때 가능한 list가 가지고 있는 next 값에 따라 다를 수 있기 때문에 순회하여 end를 찾아야 한다.
    for (uint32_t i = 0; i < count; i++) {
        prev = current;
        current = GMEM[current];
    }

    uint16_t alloc_end = prev;  // count만큼 지나서 마지막에 올 index

    manager->free_head = current; // G[current] 의 next 값

    if (manager->head == NULL_POINT) { // 처음 할당 
        manager->head = alloc_start; // 할당 하고 있는 것을 manager head에 alloc start로 넣는다.
        manager->tail = alloc_end; // alloc end를 tail로 넣는다.
        GMEM[alloc_end] = alloc_start;  // 할당하고 있는 것들은 ring 구조로 관리한다.
    } else { // 기존의 할당이 있을 때
        GMEM[manager->tail] = alloc_start;  // tail index의 다음을 start로 넣어준다.
        GMEM[alloc_end] = manager->head;    // alloc_end index를 할당 중인 head와 연결
        manager->tail = alloc_end;          
    }

    manager->free_count -= count;

    printf("%u개 노드 할당됨 (시작: %u) (head: %u, tail: %u, free_head: %u)\n",
           count, alloc_start, manager->head, manager->tail, manager->free_head);

    return alloc_start;
}


uint16_t dealloc_list(uint32_t count, uint16_t dealloc_index, LIST_MANAGER* manager) {
    if (count == 0) {
        printf("오류: 해제 개수가 0입니다\n");
        return manager->head;
    }

    if (manager->free_count + count > SIZE) {
        printf("오류: 크기 초과\n");
        return manager->head;
    }

    if (manager->head == NULL_POINT) {
        printf("오류: 해제할 할당된 노드가 없습니다\n");
        return manager->head;
    }

    // manager에서 할당 중인 head를 찾는다.
    uint16_t prev = NULL_POINT;
    uint16_t current = manager->head;
    bool found = false;

    // alloc된 index에서 next를 타고 가면서 dealloc_index를 찾는다.
    do {
        if (current == dealloc_index) { // 찾으면 found true
            found = true;
            break;
        }
        prev = current;
        current = GMEM[current];
    } while (current != manager->head);

    if (!found) {
        printf("오류: 인덱스 %u가 할당된 리스트에 없습니다\n", dealloc_index);
        return manager->head;
    }

    uint16_t dealloc_end = dealloc_index;
    uint16_t after_dealloc = NULL_POINT;
    bool tail_in_range = false;

    // dealloc_end는 순회하면서 마지막 해제 index까지 간다.
    for (uint32_t i = 0; i < count; i++) {
        if (current == manager->tail) {
            tail_in_range = true;
        }
        dealloc_end = current;
        current = GMEM[current];
    }
    // dealloc을 한 다음 index 
    after_dealloc = current;

    // 현재 이용 중인 memory 크기보다 dealloc_num이 크면 전체 메모리 해제
    if (count >= SIZE - manager->free_count) {
        manager->head = NULL_POINT;
        manager->tail = NULL_POINT;
    } else { // dealloc이 이용 중인 memory보다 작으면 head는 after_dealloc
        if (dealloc_index == manager->head) { // 이용 중인 head dealloc할 때 
            manager->head = after_dealloc; 
            if (tail_in_range) { // tail이 해제 목록에 있었다면 새롭게 링 구조 연결
                uint16_t new_tail = manager->head;
                while (GMEM[new_tail] != manager->head) {
                    new_tail = GMEM[new_tail];
                }
                manager->tail = new_tail;
            } else { 
                GMEM[manager->tail] = manager->head;  // 링 구조 연결
            }
        } else { // head가 아닌 중간이나 tail
            GMEM[prev] = after_dealloc;  // 해제 이전 index 와 dealloc 후 index 연결
            if (tail_in_range) {  // tail 을 해제 이전 index로
                manager->tail = prev;
            }
        }
    }

    GMEM[dealloc_end] = manager->free_head; // dealloc_end 위치의 next를 free head 위치와 연결
    manager->free_head = dealloc_index; // free의 head를 이제 해제한 메모리 시작 위치로

    manager->free_count += count;

    if (manager->head == NULL_POINT) {
        printf("%u개 노드 해제됨 (인덱스: %u) (모든 노드 사용 가능, free_head: %u)\n",
               count, dealloc_index, manager->free_head);
    } else {
        printf("%u개 노드 해제됨 (인덱스: %u) (head: %u, tail: %u, free_head: %u)\n",
               count, dealloc_index, manager->head, manager->tail, manager->free_head);
    }

    return manager->head;
}

void print_list(LIST_MANAGER* manager) {
    printf("\n========== 리스트 상태 ==========\n");

    if (manager->head == NULL_POINT) {
        printf("Head: 없음, Tail: 없음, Free_head: %u, 사용가능: %u, 할당됨: %u\n",
               manager->free_head, manager->free_count, SIZE - manager->free_count);
    } else {
        printf("Head: %u, Tail: %u, Free_head: %u, 사용가능: %u, 할당됨: %u\n",
               manager->head, manager->tail, manager->free_head,
               manager->free_count, SIZE - manager->free_count);
    }

    if (manager->free_count == SIZE) {
        printf("할당되지 않은 memory.\n");

        // Verify free ring structure
        printf("\n사용 가능 링 구조 검증 중 (%u 개 노드):\n", SIZE);
        uint16_t current = manager->free_head;
        bool visited[SIZE] = {false};
        uint32_t count = 0;
        
        /*
            ==================================================================
                            사이즈만큼 전체 돌았을 때 ring구조에 따라
                            current와 초기 manager의 free_head가 같아야함
            ==================================================================
        */
        while (count < SIZE) {
            if (visited[current]) {
                printf("오류: 사용 가능 노드 %u를 두 번 방문 - 링크 손상!\n", current);
                printf("=================================\n\n");
                return;
            }
            visited[current] = true;
            current = GMEM[current];
            count++;
        }

        if (current != manager->free_head) {
            printf("오류: 사용 가능 링이 불완전함 - %u에서 끝나야 하는데 %u에서 끝남\n",
                   current, manager->free_head);
        } else {
            printf("✓ 사용 가능 링 구조 정상: 모든 %u개 노드 연결됨\n", SIZE);
        }

        printf("=================================\n\n");
        return;
    }


    printf("\n 할당된 memory: ");
    uint16_t current = manager->head;
    uint32_t alloc_count = 0;

    // memory 연결 전부 출력
    do {
        printf("%u ", current);
        alloc_count++;
        if (alloc_count % 20 == 0) printf("\n                 ");
        current = GMEM[current];
    } while (current != manager->head && alloc_count < SIZE);
    printf("(개수: %u)\n", alloc_count);

    
    printf("\n할당된 링 구조 검증 중:\n");
    current = manager->head;
    bool visited_alloc[SIZE] = {false};
    uint32_t count = 0;
    uint32_t expected_alloc = SIZE - manager->free_count;

    // 방문 시 또 true가 있다면 ring 구조가 깨짐
    do {
        if (visited_alloc[current]) {
            printf("오류: 할당된 노드 %u를 두 번 방문 - 링크 손상!\n", current);
            printf("=================================\n\n");
            return;
        }
        visited_alloc[current] = true;
        current = GMEM[current];
        count++;
    } while (current != manager->head && count < SIZE);

    if (count != expected_alloc) {
        printf("ERROR: 할당된 링 %u memory, 계산한 할당된 memory %u\n", count, expected_alloc);
    } else if (current != manager->head) {
        printf("ERROR: 링 구조가 깨짐\n");
    } else {
        printf("✓ 할당된 memory: %u, 링 구조 확인\n", count);
    }

    printf("\n사용 가능 링 구조 검증 중:\n");
    current = manager->free_head;
    bool visited_free[SIZE] = {false};
    count = 0;

    while (count < manager->free_count && count < SIZE) {
        if (visited_alloc[current]) {
            printf("오류: 노드 %u가 할당된 리스트와 사용 가능 리스트 모두에 나타남!\n", current);
            printf("=================================\n\n");
            return;
        }
        if (visited_free[current]) {
            printf("오류: 사용 가능 노드 %u를 두 번 방문 - 링크 손상!\n", current);
            printf("=================================\n\n");
            return;
        }
        visited_free[current] = true;
        current = GMEM[current];
        count++;
    }

    if (count != manager->free_count) {
        printf("ERROR: Free ring has %u memory, but expected %u\n", count, manager->free_count);
    } else {
        printf("✓ Free ring 정상: %u \n", count);
    }

    // Verify all nodes accounted for
    uint32_t total = alloc_count + manager->free_count;
    if (total != SIZE) {
        printf("경고: 총 노드 수 (%u) != 전체 크기 (%u)\n", total, SIZE);
    } else {
        printf("✓ 모든 %u개 노드 확인됨\n", SIZE);
    }

    printf("=================================\n\n");
}

// Test 1: Basic operations
void test_basic_operations() {
    printf("\n\n===== 테스트 1: 기본 동작 =====\n");

    LIST_MANAGER manager;
    init_list(&manager);

    printf("\n--- 초기 상태 ---\n");
    print_list(&manager);

    printf("\n--- 노드 10개 할당 ---\n");
    uint16_t alloc1 = alloc_list(10, &manager);
    print_list(&manager);

    printf("\n--- 노드 20개 추가 할당 ---\n");
    uint16_t alloc2 = alloc_list(20, &manager);
    print_list(&manager);

    printf("\n--- 첫 할당(alloc1)에서 5개 노드 해제 ---\n");
    dealloc_list(5, alloc1, &manager);
    print_list(&manager);

    printf("\n--- 노드 15개 할당 ---\n");
    alloc_list(15, &manager);
    print_list(&manager);
}

// Test 2: Random stress test
void test_random_stress() {
    printf("\n\n===== 테스트 2: 랜덤 테스트 =====\n");

    LIST_MANAGER manager;
    init_list(&manager);

    #define MAX_ALLOCATIONS 20
    uint16_t allocations[MAX_ALLOCATIONS];
    uint32_t alloc_sizes[MAX_ALLOCATIONS];
    int num_allocations = 0;

    srand(time(NULL));

    printf("50회 무작위 작업 수행 중...\n\n");

    for (int iteration = 0; iteration < 50; iteration++) {
        int operation = rand() % 2;

        // Allocate if we have space or no allocations yet
        if ((operation == 0 && manager.free_count > 0) || num_allocations == 0) {
            uint32_t alloc_size = (rand() % 15) + 1;

            if (alloc_size <= manager.free_count && num_allocations < MAX_ALLOCATIONS) {
                uint16_t idx = alloc_list(alloc_size, &manager);

                if (idx != NULL_POINT) {
                    allocations[num_allocations] = idx;
                    alloc_sizes[num_allocations] = alloc_size;
                    num_allocations++;
                }
            }
        }
        // Deallocate
        else if (num_allocations > 0) {
            int dealloc_idx = rand() % num_allocations;
            uint16_t idx = allocations[dealloc_idx];
            uint32_t size = alloc_sizes[dealloc_idx];

            dealloc_list(size, idx, &manager);

            // Remove from tracking array
            for (int i = dealloc_idx; i < num_allocations - 1; i++) {
                allocations[i] = allocations[i + 1];
                alloc_sizes[i] = alloc_sizes[i + 1];
            }
            num_allocations--;
        }

        // Periodic verification
        if (iteration % 10 == 9) {
            printf("\n=== %d회 반복 후 검증 ===\n", iteration + 1);
            print_list(&manager);
        }
    }

    printf("\n=== 무작위 작업 후 최종 상태 ===\n");
    print_list(&manager);
}

// Test 3: Edge cases
void test_edge_cases() {
    printf("\n\n===== 테스트 3: 엣지 케이스 =====\n");

    LIST_MANAGER manager;

    printf("\n--- 엣지 케이스 1: 모든 노드 할당 ---\n");
    init_list(&manager);
    uint16_t all = alloc_list(SIZE, &manager);
    print_list(&manager);

    printf("\n--- 엣지 케이스 2: 모든 노드 해제 ---\n");
    dealloc_list(SIZE, all, &manager);
    print_list(&manager);

    printf("\n--- 엣지 케이스 3: 1개 할당, 1개 해제 (5회 반복) ---\n");
    init_list(&manager);
    for (int i = 0; i < 5; i++) {
        printf("\n반복 %d:\n", i + 1);
        uint16_t idx = alloc_list(1, &manager);
        dealloc_list(1, idx, &manager);
    }
    print_list(&manager);

    printf("\n--- 엣지 케이스 4: 단편화 테스트 ---\n");
    init_list(&manager);
    uint16_t a1 = alloc_list(30, &manager);
    uint16_t a2 = alloc_list(30, &manager);
    alloc_list(30, &manager);

    printf("\n--- 중간 블록 해제 ---\n");
    dealloc_list(30, a2, &manager);
    print_list(&manager);

    printf("\n--- 단편화 발생 후 노드 20개 할당 ---\n");
    alloc_list(20, &manager);
    print_list(&manager);
}

int main() {
    printf("리스트 테스트 시작");
    test_basic_operations();
    test_random_stress();
    test_edge_cases();

    printf("\n========================================\n");
    printf("모든 테스트 완료!\n");
    printf("========================================\n");

    return 0;
}