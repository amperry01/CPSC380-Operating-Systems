#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

float Avg;
int Max, Min;
int Arr[40];
int Count;

void *calcAvg(void *arg){
    int sum = 0;
    for (int i = 0; i < Count; i++) {
        sum += Arr[i];
    }
    
    Avg = (float)sum/(float)Count;

    return NULL;
}

void *calcMax(void *arg){
    Max = Arr[0];
    for (int i = 1; i < Count; i++) {
        if (Arr[i] > Max) {
            Max = Arr[i];
        }
    }
    return NULL;
}

void *calcMin(void *arg){
    Min = Arr[0];
    for (int i = 1; i < Count; i++) {
        if (Arr[i] < Min) {
            Min = Arr[i];
        }
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    
    pthread_t tid[3];

    Count = argc - 1;
    for (int i = 1; i < argc; i++) {
        Arr[i - 1] = atoi(argv[i]);
    }

    pthread_create(&tid[0], NULL, calcAvg, NULL);
    pthread_create(&tid[1], NULL, calcMax, NULL);
    pthread_create(&tid[2], NULL, calcMin, NULL);

    // wait for each thread to finish
    pthread_join(tid[0], NULL);
    pthread_join(tid[1], NULL);
    pthread_join(tid[2], NULL);

    printf("Average: %f \n", Avg);
    printf("Maximum: %d \n", Max);
    printf("Minimum: %d \n", Min);

    return 0;
}