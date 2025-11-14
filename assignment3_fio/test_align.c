#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    void *ptr = NULL;
    int ret = posix_memalign(&ptr, 12288, 12288);
    printf("posix_memalign with alignment 12288: ret=%d (%s)\n", ret, strerror(ret));
    if (ptr) free(ptr);
    
    ptr = NULL;
    ret = posix_memalign(&ptr, 4096, 12288);
    printf("posix_memalign with alignment 4096: ret=%d (%s)\n", ret, strerror(ret));
    if (ptr) free(ptr);
    
    return 0;
}
