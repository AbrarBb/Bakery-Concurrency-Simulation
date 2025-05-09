/*
 * Sweet Harmony Bakery - Data Structures and Synchronization Design
 * 
 * This file implements the necessary data structures and synchronization 
 * mechanisms to model the Sweet Harmony bakery scenario, where:
 * 1. Equal numbers of red and blue outfit customers must be maintained
 * 2. Limited tables must be managed
 * 3. Customer queues must be handled
 * 4. Entry/exit operations must be synchronized
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <stdbool.h>

/* Constants */
#define MAX_TABLES 10            // Total number of tables in the bakery
#define MAX_CUSTOMERS 100        // Maximum number of customers to simulate

/* Color enumeration */
typedef enum {
    RED = 0,
    BLUE = 1
} CustomerColor;

/* Customer structure */
typedef struct {
    int id;                      // Unique customer ID
    CustomerColor color;         // RED or BLUE outfit
    int eating_time;             // Time spent at the table in seconds
    bool has_table;              // Whether customer is seated at a table
} Customer;

/* Bakery state structure */
typedef struct {
    int customers_inside;        // Total customers inside
    int red_count;               // Customers wearing red
    int blue_count;              // Customers wearing blue
    int free_tables;             // Available tables
    bool tables[MAX_TABLES];     // Table occupancy (true if occupied)
    
    // Queue structures for waiting customers
    Customer* red_queue[MAX_CUSTOMERS];
    int red_queue_front;
    int red_queue_rear;
    int red_queue_size;
    
    Customer* blue_queue[MAX_CUSTOMERS];
    int blue_queue_front;
    int blue_queue_rear;
    int blue_queue_size;
    
    // Synchronization objects
    pthread_mutex_t bakery_mutex;    // Protects the bakery state
    sem_t red_sem;                   // Controls red customer entry
    sem_t blue_sem;                  // Controls blue customer entry
    sem_t tables_sem;                // Controls table availability
    pthread_cond_t balance_cond;     // Signals when color balance changes
} BakeryState;

/* Global state */
BakeryState bakery;

/* Function prototypes */
void init_bakery(int total_tables);
void cleanup_bakery();
int find_free_table();
void* customer_behavior(void* arg);
void enqueue_customer(Customer* customer);
Customer* dequeue_customer(CustomerColor color);
bool can_enter(CustomerColor color);
void try_balance_entry();

/* Initialize bakery state and synchronization objects */
void init_bakery(int total_tables) {
    bakery.customers_inside = 0;
    bakery.red_count = 0;
    bakery.blue_count = 0;
    bakery.free_tables = total_tables;
    
    // Initialize table states
    for (int i = 0; i < MAX_TABLES; i++) {
        bakery.tables[i] = false;  // All tables start unoccupied
    }
    
    // Initialize queues
    bakery.red_queue_front = 0;
    bakery.red_queue_rear = -1;
    bakery.red_queue_size = 0;
    
    bakery.blue_queue_front = 0;
    bakery.blue_queue_rear = -1;
    bakery.blue_queue_size = 0;
    
    // Initialize synchronization objects
    pthread_mutex_init(&bakery.bakery_mutex, NULL);
    sem_init(&bakery.red_sem, 0, 0);  // Start with no permits
    sem_init(&bakery.blue_sem, 0, 0);  // Start with no permits
    sem_init(&bakery.tables_sem, 0, total_tables);  // Start with all tables free
    pthread_cond_init(&bakery.balance_cond, NULL);
}

/* Clean up resources */
void cleanup_bakery() {
    pthread_mutex_destroy(&bakery.bakery_mutex);
    sem_destroy(&bakery.red_sem);
    sem_destroy(&bakery.blue_sem);
    sem_destroy(&bakery.tables_sem);
    pthread_cond_destroy(&bakery.balance_cond);
}

/* Find an available table; returns table ID or -1 if none available */
int find_free_table() {
    for (int i = 0; i < MAX_TABLES; i++) {
        if (!bakery.tables[i]) {
            return i;
        }
    }
    return -1;
}

/* Enqueue a customer in their color's queue */
void enqueue_customer(Customer* customer) {
    if (customer->color == RED) {
        bakery.red_queue_rear = (bakery.red_queue_rear + 1) % MAX_CUSTOMERS;
        bakery.red_queue[bakery.red_queue_rear] = customer;
        bakery.red_queue_size++;
    } else {
        bakery.blue_queue_rear = (bakery.blue_queue_rear + 1) % MAX_CUSTOMERS;
        bakery.blue_queue[bakery.blue_queue_rear] = customer;
        bakery.blue_queue_size++;
    }
}

/* Dequeue a customer from their color's queue */
Customer* dequeue_customer(CustomerColor color) {
    Customer* customer = NULL;
    
    if (color == RED && bakery.red_queue_size > 0) {
        customer = bakery.red_queue[bakery.red_queue_front];
        bakery.red_queue_front = (bakery.red_queue_front + 1) % MAX_CUSTOMERS;
        bakery.red_queue_size--;
    } else if (color == BLUE && bakery.blue_queue_size > 0) {
        customer = bakery.blue_queue[bakery.blue_queue_front];
        bakery.blue_queue_front = (bakery.blue_queue_front + 1) % MAX_CUSTOMERS;
        bakery.blue_queue_size--;
    }
    
    return customer;
}

/* Check if a customer of given color can enter based on balance rule */
bool can_enter(CustomerColor color) {
    if (bakery.customers_inside == 0) {
        // First customer can always enter
        return true;
    }
    
    if (color == RED) {
        // Red customer can enter if doing so maintains or improves balance
        return bakery.red_count < bakery.blue_count;
    } else {
        // Blue customer can enter if doing so maintains or improves balance
        return bakery.blue_count < bakery.red_count;
    }
}

/* Try to maintain balance by allowing customers to enter */
void try_balance_entry() {
    // Check if we can let more customers in to balance colors
    while (bakery.free_tables > 0) {
        if (bakery.red_count < bakery.blue_count && bakery.red_queue_size > 0) {
            // Let a red customer in
            sem_post(&bakery.red_sem);
            break;
        } else if (bakery.blue_count < bakery.red_count && bakery.blue_queue_size > 0) {
            // Let a blue customer in
            sem_post(&bakery.blue_sem);
            break;
        } else if (bakery.red_count == bakery.blue_count) {
            // Colors are balanced, we can let either color in
            if (bakery.red_queue_size > 0) {
                sem_post(&bakery.red_sem);
                break;
            } else if (bakery.blue_queue_size > 0) {
                sem_post(&bakery.blue_sem);
                break;
            } else {
                // No customers waiting
                break;
            }
        } else {
            // Can't improve balance right now
            break;
        }
    }
}

/* Customer thread behavior */
void* customer_behavior(void* arg) {
    Customer* customer = (Customer*)arg;
    int table_id = -1;
    
    printf("Customer %d (%s) arrives at Sweet Harmony.\n", 
           customer->id, customer->color == RED ? "RED" : "BLUE");
    
    // Try to enter bakery
    pthread_mutex_lock(&bakery.bakery_mutex);
    
    // Check if customer can enter immediately
    if (bakery.free_tables > 0 && can_enter(customer->color)) {
        // Customer can enter directly
        if (customer->color == RED) {
            bakery.red_count++;
        } else {
            bakery.blue_count++;
        }
        
        bakery.customers_inside++;
        bakery.free_tables--;
        table_id = find_free_table();
        bakery.tables[table_id] = true;
        customer->has_table = true;
        
        printf("Customer %d (%s) enters and sits at table %d. Inside: %d red, %d blue\n", 
               customer->id, customer->color == RED ? "RED" : "BLUE", 
               table_id, bakery.red_count, bakery.blue_count);
        
        pthread_mutex_unlock(&bakery.bakery_mutex);
    } else {
        // Customer must wait in queue
        printf("Customer %d (%s) waits in line.\n", 
               customer->id, customer->color == RED ? "RED" : "BLUE");
        
        enqueue_customer(customer);
        pthread_mutex_unlock(&bakery.bakery_mutex);
        
        // Wait for permission to enter
        if (customer->color == RED) {
            sem_wait(&bakery.red_sem);
        } else {
            sem_wait(&bakery.blue_sem);
        }
        
        // Customer is now allowed to enter
        pthread_mutex_lock(&bakery.bakery_mutex);
        
        // Validate we have a table (might have changed while waiting)
        if (bakery.free_tables > 0) {
            // Update bakery state
            if (customer->color == RED) {
                bakery.red_count++;
            } else {
                bakery.blue_count++;
            }
            
            bakery.customers_inside++;
            bakery.free_tables--;
            table_id = find_free_table();
            bakery.tables[table_id] = true;
            customer->has_table = true;
            
            printf("Customer %d (%s) enters from queue and sits at table %d. Inside: %d red, %d blue\n", 
                   customer->id, customer->color == RED ? "RED" : "BLUE", 
                   table_id, bakery.red_count, bakery.blue_count);
        }
        
        pthread_mutex_unlock(&bakery.bakery_mutex);
    }
    
    // Enjoy pastries for some time
    if (customer->has_table) {
        sleep(customer->eating_time);
        
        // Customer leaves
        pthread_mutex_lock(&bakery.bakery_mutex);
        
        // Update bakery state
        if (customer->color == RED) {
            bakery.red_count--;
        } else {
            bakery.blue_count--;
        }
        
        bakery.customers_inside--;
        bakery.free_tables++;
        bakery.tables[table_id] = false;
        
        printf("Customer %d (%s) leaves table %d. Inside: %d red, %d blue\n", 
               customer->id, customer->color == RED ? "RED" : "BLUE", 
               table_id, bakery.red_count, bakery.blue_count);
        
        // Try to let waiting customers in
        try_balance_entry();
        
        pthread_mutex_unlock(&bakery.bakery_mutex);
    }
    
    free(customer);
    return NULL;
}

/* Main function - example usage */
int main() {
    // Initialize the bakery with 5 tables
    init_bakery(5);
    
    pthread_t threads[MAX_CUSTOMERS];
    int customer_count = 20;  // Create 20 customers for simulation
    
    // Create customers with alternating colors
    for (int i = 0; i < customer_count; i++) {
        Customer* customer = (Customer*)malloc(sizeof(Customer));
        customer->id = i + 1;
        customer->color = i % 2 == 0 ? RED : BLUE;  // Alternate red and blue
        customer->eating_time = rand() % 5 + 1;     // Random eating time 1-5 seconds
        customer->has_table = false;
        
        pthread_create(&threads[i], NULL, customer_behavior, (void*)customer);
        
        // Small delay between customer arrivals
        usleep(500000);  // 0.5 seconds
    }
    
    // Wait for all customer threads to finish
    for (int i = 0; i < customer_count; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Clean up resources
    cleanup_bakery();
    
    printf("Sweet Harmony bakery is now closed.\n");
    return 0;
}
