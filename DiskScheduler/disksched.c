#include <stdio.h>
#include <stdlib.h>

int requests[] = {2069, 1212, 2296, 2800, 544, 1618, 356, 1523, 4965, 3681};
const int max_cylinders = 5000;
const int num_requests = 10;

int main(int argc, char *argv[]){
    int total_movement = 0; // total cylinders (cylinders moved)

    if (argc != 2) {
        printf("Usage: %s <initial_head_position>\n", argv[0]);
        return 1;
    }

    int initial_head = atoi(argv[1]);
    if (initial_head < 0 || initial_head >= max_cylinders) {
        printf("Initial head position must be between 0 and %d\n", max_cylinders - 1);
        return 1;
    }

    for (int i = 0; i < num_requests; i++) {
        total_movement += abs(requests[i] - initial_head);
        initial_head = requests[i];
    }

    printf("Total head movement: %d cylinders\n", total_movement);

    return 0;
}