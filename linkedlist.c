#include <stdio.h>
#include <stdint.h>
#define SIZE 128
#define NULL_INDEX 0xFFFF

typedef struct{
    uint32_t data;
    uint16_t prev;
    uint16_t next;
}NODE_T;

typedef struct {
    uint16_t free_head;
    uint16_t free_tail;
    uint32_t free_count;
}LIST_MANAGER;

typedef struct{
    uint16_t head;
    uint32_t size; // 4바이트 연산
} LIST_INFO;

LIST_MANAGER list_manager;
NODE_T GMEM[SIZE];
uint8_t GMEM_ALLOCATED[SIZE];

// 함수 프로토타입
void init_list();
LIST_INFO alloc_list(LIST_MANAGER *list_manager, uint32_t alloc_num);
void dealloc_list(LIST_MANAGER *list_manager, LIST_INFO *list_info, uint32_t dealloc_num);
void print_list();
void print_allocated_list(uint16_t head);
void print_free_list();
void print_list_info(LIST_INFO *list_info, const char *name);

int main(void)
{
    init_list();

    printf("=== 초기 상태 ===");
    print_list();

    printf("\n=== 5개 노드 할당 ===");
    LIST_INFO list1 = alloc_list(&list_manager, 5);
    if(list1.head != NULL_INDEX) {
        // 할당된 노드에 데이터 설정
        uint16_t curr = list1.head;
        for(int i = 1; i <= 5; i++) {
            GMEM[curr].data = i * 10;
            curr = GMEM[curr].next;
        }
    }
    print_list();
    print_list_info(&list1, "List1");

    // 3개 노드 더 할당
    printf("\n=== 3개 노드 추가 할당 ===");
    LIST_INFO list2 = alloc_list(&list_manager, 3);
    if(list2.head != NULL_INDEX) {
        uint16_t curr = list2.head;
        for(int i = 1; i <= 3; i++) {
            GMEM[curr].data = i * 100;
            curr = GMEM[curr].next;
        }
    }
    print_list();
    print_list_info(&list2, "List2");

    // 부분 해제 테스트: 5개 중 3개만 해제
    printf("\n=== 첫 번째 리스트에서 3개만 해제 (5개 중) ===");
    dealloc_list(&list_manager, &list1, 3);
    print_list();
    print_list_info(&list1, "List1 (부분 해제 후)");

    // 남은 노드 전부 해제
    printf("\n=== 남은 노드 전부 해제 ===");
    dealloc_list(&list_manager, &list1, list1.size);  // 전체 크기 사용
    print_list();
    print_list_info(&list1, "List1 (전체 해제 후)");

    printf("\n=== 두 번째 할당(3개) 중 2개만 해제 ===");
    dealloc_list(&list_manager, &list2, 2);
    print_list();
    print_list_info(&list2, "List2 (부분 해제 후)");

    printf("\n=== 남은 노드 해제 ===");
    dealloc_list(&list_manager, &list2, list2.size);  // 전체 크기 사용
    print_list();
    print_list_info(&list2, "List2 (전체 해제 후)");

    printf("\n=== 10개 새로 할당 ===");
    LIST_INFO list3 = alloc_list(&list_manager, 10);
    if(list3.head != NULL_INDEX) {
        uint16_t curr = list3.head;
        for(int i = 1; i <= 10; i++) {
            GMEM[curr].data = i;
            curr = GMEM[curr].next;
        }
    }
    print_list();
    print_list_info(&list3, "List3");

    // 부분 해제 후 남은 리스트 테스트
    printf("\n=== 10개 중 7개 해제하고 남은 3개 출력 ===");
    dealloc_list(&list_manager, &list3, 7);
    print_list();
    print_list_info(&list3, "List3 (부분 해제 후)");

    return 0;
}

// 리스트 초기화: 모든 노드를 free list에 연결
void init_list()
{
    list_manager.free_head = 0;
    list_manager.free_tail = SIZE - 1;
    list_manager.free_count = SIZE;
    
    // 모든 노드를 free list로 연결
    for(int i = 0; i < SIZE; i++)
    {
        GMEM[i].data = 0;
        GMEM[i].prev = (i == 0) ? NULL_INDEX : (i - 1);
        GMEM[i].next = (i == SIZE - 1) ? NULL_INDEX : (i + 1);
        GMEM_ALLOCATED[i] = 0;
    }
}

// 노드 할당: free list에서 노드를 꺼내서 새로운 리스트 생성
LIST_INFO alloc_list(LIST_MANAGER *list_manager, uint32_t alloc_num)
{
    LIST_INFO result = {NULL_INDEX, 0};

    // 입력 유효성 검사
    if(alloc_num == 0 || alloc_num > list_manager->free_count)
    {
        printf("Error: 할당 불가 (요청: %u, 사용가능: %u)\n",
               alloc_num, list_manager->free_count);
        return result;
    }

    uint16_t new_list_head = NULL_INDEX;
    uint16_t prev_node = NULL_INDEX;

    // alloc_num 개수만큼 free list에서 노드 가져오기
    for(uint32_t i = 0; i < alloc_num; i++)
    {
        // Free list에서 head 노드 추출
        uint16_t curr_node = list_manager->free_head;
        uint16_t next_free = GMEM[curr_node].next;

        // Free list 업데이트
        list_manager->free_head = next_free;
        if(next_free != NULL_INDEX)
        {
            GMEM[next_free].prev = NULL_INDEX;
        }
        else
        {
            list_manager->free_tail = NULL_INDEX;
        }

        // 새 리스트에 노드 추가
        GMEM[curr_node].data = 0;
        GMEM[curr_node].prev = prev_node;
        GMEM[curr_node].next = NULL_INDEX;
        GMEM_ALLOCATED[curr_node] = 1;

        // 이전 노드와 연결
        if(prev_node != NULL_INDEX)
        {
            GMEM[prev_node].next = curr_node;
        }
        else
        {
            new_list_head = curr_node;  // 첫 번째 노드
        }

        prev_node = curr_node;
    }

    list_manager->free_count -= alloc_num;

    result.head = new_list_head;
    result.size = alloc_num;
    return result;
}



void dealloc_list(LIST_MANAGER *list_manager, LIST_INFO *list_info, uint32_t dealloc_num)
{
    if(list_info == NULL || list_info->head == NULL_INDEX || dealloc_num == 0)
    {
        printf("Error: 유효하지 않은 리스트 또는 해제 개수\n");
        return;
    }

    // 해제 개수가 리스트 크기보다 크면 전체 크기로 조정
    if(dealloc_num > list_info->size)
    {
        printf("Warning: 해제 요청 개수(%u)가 리스트 크기(%u)보다 큽니다. 전체를 해제합니다.\n",
               dealloc_num, list_info->size);
        dealloc_num = list_info->size;
    }

    uint16_t current = list_info->head;
    uint16_t first_dealloc = list_info->head;
    uint16_t last_dealloc = NULL_INDEX;
    uint32_t actual_count = 0;


    for(uint32_t i = 0; i < dealloc_num && current != NULL_INDEX; i++)
    {
        if(!GMEM_ALLOCATED[current])
        {
            printf("Warning: 노드 %u는 이미 해제되어 있습니다.\n", current);
            current = GMEM[current].next;
            continue;
        }

        last_dealloc = current;
        uint16_t next = GMEM[current].next;

        // 노드 초기화
        GMEM[current].data = 0;
        GMEM_ALLOCATED[current] = 0;

        // 다음 노드로 이동
        current = next;
        actual_count++;
    }

    if(actual_count == 0)
    {
        printf("해제할 노드가 없습니다.\n");
        return;
    }

    // 남아있는 노드가 있다면 해당 노드를 새로운 head로 설정
    uint16_t new_head = current;
    if(new_head != NULL_INDEX)
    {
        // 남은 리스트의 첫 노드의 prev를 NULL_INDEX로 설정
        GMEM[new_head].prev = NULL_INDEX;
    }

    // 해제된 노드들을 free list의 tail에 추가
    if(list_manager->free_tail != NULL_INDEX)
    {
        GMEM[list_manager->free_tail].next = first_dealloc;
        GMEM[first_dealloc].prev = list_manager->free_tail;
    }
    else
    {
        list_manager->free_head = first_dealloc;
        GMEM[first_dealloc].prev = NULL_INDEX;
    }

    GMEM[last_dealloc].next = NULL_INDEX;
    list_manager->free_tail = last_dealloc;
    list_manager->free_count += actual_count;

    // LIST_INFO 업데이트
    list_info->head = new_head;
    list_info->size -= actual_count;

    printf("%u개 노드 해제 완료", actual_count);
    if(new_head != NULL_INDEX)
    {
        printf(", 남은 리스트: head=%u, size=%u\n", new_head, list_info->size);
    }
    else
    {
        printf(", 모든 노드 해제됨 (size=0)\n");
    }
}

// 할당된 리스트 출력
void print_allocated_list(uint16_t head)
{
    if(head == NULL_INDEX)
    {
        printf("할당된 리스트가 없습니다.\n");
        return;
    }
    
    printf("할당된 리스트: ");
    uint16_t current = head;
    int count = 0;
    while(current != NULL_INDEX && count < 20)  // 무한루프 방지
    {
        printf("[idx:%u, data:%u] ", current, GMEM[current].data);
        current = GMEM[current].next;
        count++;
    }
    printf("\n");
}

// Free list 출력
void print_free_list()
{
    uint16_t current = list_manager.free_head;
    int count = 0;
    while(current != NULL_INDEX && count < 10)
    {
        printf("%u ", current);
        current = GMEM[current].next;
        count++;
    }
    if(list_manager.free_count > 10)
    {
        printf("... (총 %u개)", list_manager.free_count);
    }
    printf("\n");
}

// 전체 상태 출력
void print_list()
{
    printf("\n=== List Manager State ===\n");
    printf("Free count: %u\n", list_manager.free_count);
    printf("Free list head: %u, tail: %u\n",
           list_manager.free_head, list_manager.free_tail);

    print_free_list();

    printf("Index | Data | Prev | Next | Allocated\n");
    printf("------|------|------|------|-----------\n");
    for(int i = 0; i < 15 && i < SIZE; i++)
    {
        printf("%5d | %4u | %4d | %4d | %9s\n",
               i,
               GMEM[i].data,
               GMEM[i].prev == NULL_INDEX ? -1 : GMEM[i].prev,
               GMEM[i].next == NULL_INDEX ? -1 : GMEM[i].next,
               GMEM_ALLOCATED[i] ? "Yes" : "No");
    }
    printf("\n");
}

// LIST_INFO 출력
void print_list_info(LIST_INFO *list_info, const char *name)
{
    if(list_info == NULL)
    {
        printf("%s: NULL\n", name);
        return;
    }

    if(list_info->head == NULL_INDEX)
    {
        printf("%s: (빈 리스트, size=0)\n", name);
        return;
    }

    printf("%s: head=%u, size=%u - ", name, list_info->head, list_info->size);
    print_allocated_list(list_info->head);
}