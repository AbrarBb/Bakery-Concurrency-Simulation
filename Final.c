#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

#define RED_COUNT 3
#define BLUE_COUNT 3
#define TABLES 2

int red_inside = 0, blue_inside = 0;
pthread_mutex_t mutex;
sem_t table_sem;

void* red_customer(void* arg) 
{
    int id = *(int*)arg;
    free(arg);

    while (1) 
    {
        pthread_mutex_lock(&mutex);
        if (red_inside < blue_inside) 
        {
            red_inside++;
            printf("🔴 Red %d entered (R=%d, B=%d)\n", id, red_inside, blue_inside);
            pthread_mutex_unlock(&mutex);
            break;
        } else if (red_inside == 0 && blue_inside == 0) {
            red_inside++;
            printf("🔴 Red %d (first red) entered\n", id);
            pthread_mutex_unlock(&mutex);
            break;
        }
        pthread_mutex_unlock(&mutex);
        usleep(100000); // retry later
    }

    sem_wait(&table_sem);
    printf("🔴 Red %d got a table\n", id);
    sleep(1);
    printf("🔴 Red %d leaving\n", id);

    pthread_mutex_lock(&mutex);
    red_inside--;
    pthread_mutex_unlock(&mutex);
    sem_post(&table_sem);
    return NULL;
}

void* blue_customer(void* arg) 
{
    int id = *(int*)arg;
    free(arg);

    while (1) 
    {
        pthread_mutex_lock(&mutex);
        if (blue_inside < red_inside) 
        {
            blue_inside++;
            printf("🔵 Blue %d entered (R=%d, B=%d)\n", id, red_inside, blue_inside);
            pthread_mutex_unlock(&mutex);
            break;
        } 
        else if (red_inside == 0 && blue_inside == 0) 
        {
            blue_inside++;
            printf("🔵 Blue %d (first blue) entered\n", id);
            pthread_mutex_unlock(&mutex);
            break;
        }
        pthread_mutex_unlock(&mutex);
        usleep(100000); // retry later
    }

    sem_wait(&table_sem);
    printf("🔵 Blue %d got a table\n", id);
    sleep(1);
    printf("🔵 Blue %d leaving\n", id);

    pthread_mutex_lock(&mutex);
    blue_inside--;
    pthread_mutex_unlock(&mutex);
    sem_post(&table_sem);
    return NULL;
}

int main() 
{
    pthread_t red[RED_COUNT], blue[BLUE_COUNT];
    pthread_mutex_init(&mutex, NULL);
    sem_init(&table_sem, 0, TABLES);

    for (int a = 0; a < RED_COUNT; a++) 
    {
        int* id = malloc(sizeof(int));
        *id = a + 1;
        pthread_create(&red[a], NULL, red_customer, id);
        usleep(100000);
    }

    for (int b = 0; b < BLUE_COUNT; b++) 
    {
        int* id = malloc(sizeof(int));
        *id = b + 1;
        pthread_create(&blue[b], NULL, blue_customer, id);
        usleep(100000);
    }

    for (int a = 0; a < RED_COUNT; a++) pthread_join(red[a], NULL);
    for (int b = 0; b < BLUE_COUNT; b++) pthread_join(blue[b], NULL);

    pthread_mutex_destroy(&mutex);
    sem_destroy(&table_sem);
    printf("\n🎉 All customers served. Bakery closed.\n");
    return 0;
}
