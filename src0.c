#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

#define TABLES 3
#define CUSTOMERS 10

sem_t red_light, blue_light;  // Traffic lights for outfits
sem_t available_tables;       // Counting available tables
pthread_mutex_t lock;         // For safe counting

int reds = 0, blues = 0;      // Current customers by color

void* customer(void* arg) 
{
    int id = *(int*)arg;
    int is_red = id % 2;      // Even IDs are red, odd are blue
    
    printf("Customer %d (%s) arrives\n", id, is_red ? "RED" : "BLUE");
    
    // Wait for matching customer
    if (is_red) 
    {
        sem_wait(&blue_light); // Need a blue to balance
        reds++;
        sem_post(&red_light);  // Signal red is available
    } else {
        sem_wait(&red_light);  // Need a red to balance
        blues++;
        sem_post(&blue_light); // Signal blue is available
    }
    
    // Get a table
    sem_wait(&available_tables);
    printf("Customer %d (%s) sits down\n", id, is_red ? "RED" : "BLUE");
    
    // Enjoy pastry
    sleep(1 + id % 3);
    
    // Leave table
    sem_post(&available_tables);
    printf("Customer %d (%s) leaves\n", id, is_red ? "RED" : "BLUE");
    
    // Update counts
    pthread_mutex_lock(&lock);
    if (is_red) reds--; else blues--;
    pthread_mutex_unlock(&lock);
    
    // Allow new pairs
    if (is_red) sem_post(&blue_light); else sem_post(&red_light);
    
    return NULL;
}

int main() 
{
    pthread_t threads[CUSTOMERS];
    int ids[CUSTOMERS];
    
    // Initialize semaphores
    sem_init(&red_light, 0, 0);
    sem_init(&blue_light, 0, 0);
    sem_init(&available_tables, 0, TABLES);
    pthread_mutex_init(&lock, NULL);
    
    // Create customers
    for (int i = 0; i < CUSTOMERS; i++) 
    {
        ids[i] = i;
        pthread_create(&threads[i], NULL, customer, &ids[i]);
        sleep(1); // Space out arrivals
    }
    
    // Wait for all customers
    for (int i = 0; i < CUSTOMERS; i++) 
    {
        pthread_join(threads[i], NULL);
    }
    
    // Clean up
    sem_destroy(&red_light);
    sem_destroy(&blue_light);
    sem_destroy(&available_tables);
    pthread_mutex_destroy(&lock);
    
    return 0;
}
