#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <string.h>

int RED_COUNT = 3;
int BLUE_COUNT = 3;
int TABLES = 2;
int EATING_TIME = 1;

int red_inside = 0, blue_inside = 0;
int red_served = 0, blue_served = 0;
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
        } 
        else if (red_inside == 0 && blue_inside == 0) 
        {
            red_inside++;
            printf("🔴 Red %d (first customer) entered (R=%d, B=%d)\n", id, red_inside, blue_inside);
            pthread_mutex_unlock(&mutex);
            break;
        }
        pthread_mutex_unlock(&mutex);
        printf("🔴 Red %d waiting to enter...\n", id);
        usleep(100000);
    }
    
    printf("🔴 Red %d waiting for a table...\n", id);
    sem_wait(&table_sem);
    printf("🔴 Red %d got a table\n", id);
    
    sleep(EATING_TIME);
    
    printf("🔴 Red %d leaving\n", id);
    pthread_mutex_lock(&mutex);
    red_inside--;
    red_served++;
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
            printf("🔵 Blue %d (first customer) entered (R=%d, B=%d)\n", id, red_inside, blue_inside);
            pthread_mutex_unlock(&mutex);
            break;
        }
        pthread_mutex_unlock(&mutex);
        printf("🔵 Blue %d waiting to enter...\n", id);
        usleep(100000);
    }
    
    printf("🔵 Blue %d waiting for a table...\n", id);
    sem_wait(&table_sem);
    printf("🔵 Blue %d got a table\n", id);
    
    sleep(EATING_TIME);
    
    printf("🔵 Blue %d leaving\n", id);
    pthread_mutex_lock(&mutex);
    blue_inside--;
    blue_served++;
    pthread_mutex_unlock(&mutex);
    sem_post(&table_sem);
    
    return NULL;
}

int get_positive_integer(const char* prompt) 
{
    int value = 0;
    char input[100];
    
    while (1) 
    {
        printf("%s", prompt);
        if (fgets(input, sizeof(input), stdin) == NULL) 
        {
            printf("Error reading input. Using default value.\n");
            return 1;
        }
        
        value = atoi(input);
        
        if (value > 0) {
            break;
        } else {
            printf("Please enter a positive integer.\n");
        }
    }
    
    return value;
}

int main() 
{
    printf("🍰 Bakery Simulation Setup 🍰\n\n");
    
    TABLES = get_positive_integer("Enter number of tables: ");
    RED_COUNT = get_positive_integer("Enter number of red customers: ");
    BLUE_COUNT = get_positive_integer("Enter number of blue customers: ");
    EATING_TIME = get_positive_integer("Enter eating time (in seconds): ");
    
    printf("\n🍰 Starting Bakery Simulation 🍰\n");
    printf("Red customers: %d\n", RED_COUNT);
    printf("Blue customers: %d\n", BLUE_COUNT);
    printf("Available tables: %d\n", TABLES);
    printf("Eating time: %d second(s)\n\n", EATING_TIME);
    
    pthread_t red[RED_COUNT], blue[BLUE_COUNT];
    pthread_mutex_init(&mutex, NULL);
    sem_init(&table_sem, 0, TABLES);
    
    for (int a = 0; a < RED_COUNT; a++) 
    {
        int* id = malloc(sizeof(int));
        if (id == NULL) {
            perror("Error allocating memory");
            return 1;
        }
        *id = a + 1;
        if (pthread_create(&red[a], NULL, red_customer, id) != 0) {
            perror("Error creating red customer thread");
            free(id);
            return 1;
        }
        usleep(100000);
    }
    
    for (int b = 0; b < BLUE_COUNT; b++) 
    {
        int* id = malloc(sizeof(int));
        if (id == NULL) {
            perror("Error allocating memory");
            return 1;
        }
        *id = b + 1;
        if (pthread_create(&blue[b], NULL, blue_customer, id) != 0) {
            perror("Error creating blue customer thread");
            free(id);
            return 1;
        }
        usleep(100000);
    }
    
    for (int a = 0; a < RED_COUNT; a++) 
    {
        pthread_join(red[a], NULL);
    }
    for (int b = 0; b < BLUE_COUNT; b++) 
    {
        pthread_join(blue[b], NULL);
    }
    
    pthread_mutex_destroy(&mutex);
    sem_destroy(&table_sem);
    
    printf("\n🎉 All customers served. Bakery closed.\n");
    printf("Summary:\n");
    printf("- Red customers served: %d\n", red_served);
    printf("- Blue customers served: %d\n", blue_served);
    printf("- Total customers: %d\n", red_served + blue_served);
    
    return 0;
}
