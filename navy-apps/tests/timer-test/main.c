#include <NDL.h>
#include <stdio.h>

int main() {
    NDL_Init(0);
    uint32_t last_ms = NDL_GetTicks();
    int count = 0;

    printf("Timer test (NDL) start...\n");

    while (count < 10) {
        uint32_t cur_ms = NDL_GetTicks();
        if (cur_ms - last_ms >= 500) {
            printf("NDL Timer: 0.5s passed (Count: %d)\n", ++count);
            last_ms = cur_ms;
        }
    }
    NDL_Quit();
    return 0;
}