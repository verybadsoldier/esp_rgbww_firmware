#include <RGBWWCtrl.h>
#include <cstdlib>

size_t getLargestFreeHeapBlock() {
    size_t lower = 0;
    size_t upper = system_get_free_heap_size();
    size_t max_free = 0;

    // binary searching for the largest allocatable block
    while (lower <= upper) {
        size_t mid = lower + ((upper - lower) / 2);

        void* p = malloc(mid);
        if (p != nullptr) {
            free(p);
            max_free = mid;
            lower = mid + 1;
        } else {
            upper = mid - 1;
        }
    }

    return max_free;
}
