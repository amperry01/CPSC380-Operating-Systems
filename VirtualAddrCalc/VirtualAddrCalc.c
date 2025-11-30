#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {

    uint32_t logical_addr = (uint32_t)atoi(argv[1]);

    if (argc != 2) {
        printf("Usage: %s <logical_address>\n", argv[0]);
        return -1;
    }

    uint32_t page_no = logical_addr >> 12; // assuming a page size of 4KB (2^12)
    uint32_t offset = logical_addr & 0x00000FFF; //  mask to get the offset within the page by using bitwise AND with 0x00000FFF (which is 4095 in decimal because 4KB - 1 = 4095)

    printf("Page Number: %d\n", page_no);
    printf("Offset: %d\n", offset);

    return 0;
}