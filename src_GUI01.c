#include <gtk/gtk.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>

// Forward declarations for callback functions
void on_add_red_clicked(GtkWidget *widget, gpointer data);
void on_add_blue_clicked(GtkWidget *widget, gpointer data);

// Configuration
#define NUM_TABLES 5
#define MAX_CUSTOMERS 30
#define MAX_QUEUE_SIZE 20
#define CUSTOMER_STAY_MIN 3  // Minimum time a customer stays (seconds)
#define CUSTOMER_STAY_MAX 8  // Maximum time a customer stays (seconds)
#define NEW_CUSTOMER_INTERVAL 2  // New customer arrives every X seconds

// Semaphores and mutexes
sem_t red_sem;        // Controls red customers entry
sem_t blue_sem;       // Controls blue customers entry
sem_t tables_sem;     // Controls available tables
pthread_mutex_t bakery_mutex = PTHREAD_MUTEX_INITIALIZER;  // Protects shared data
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;   // Protects queue data

// Bakery state
int red_count = 0;    // Number of red customers in bakery
int blue_count = 0;   // Number of blue customers in bakery
int tables_used = 0;  // Number of tables currently in use
bool running = true;  // Global flag to control simulation

// Queue state
typedef enum { RED, BLUE } CustomerColor;

typedef struct {
    int id;
    CustomerColor color;
    bool in_bakery;
    bool at_table;
    int table_num;
    GtkWidget *label;
    GtkWidget *customer_widget;
} Customer;

Customer customers[MAX_CUSTOMERS];
int customer_count = 0;

// Queue of waiting customers
typedef struct {
    int customer_ids[MAX_QUEUE_SIZE];
    int front;
    int rear;
    int size;
} Queue;

Queue red_queue = {.front = 0, .rear = -1, .size = 0};
Queue blue_queue = {.front = 0, .rear = -1, .size = 0};

// GTK widgets
GtkWidget *window;
GtkWidget *red_count_label;
GtkWidget *blue_count_label;
GtkWidget *tables_label;
GtkWidget *red_queue_label;
GtkWidget *blue_queue_label;
GtkWidget *bakery_grid;
GtkWidget *queue_box;
GtkWidget *status_label;
GtkCssProvider *provider;

// Forward declarations
gboolean update_ui(gpointer data);
void create_customer(CustomerColor color);
void *customer_thread(void *arg);
void remove_customer_from_bakery(Customer *customer);
gboolean create_customer_widget_idle(gpointer data);
gboolean move_customer_to_bakery_idle(gpointer data);
gboolean remove_customer_from_bakery_idle(gpointer data);

// Queue functions
void enqueue(Queue *queue, int customer_id) {
    pthread_mutex_lock(&queue_mutex);
    if (queue->size < MAX_QUEUE_SIZE) {
        queue->rear = (queue->rear + 1) % MAX_QUEUE_SIZE;
        queue->customer_ids[queue->rear] = customer_id;
        queue->size++;
    }
    pthread_mutex_unlock(&queue_mutex);
}

int dequeue(Queue *queue) {
    pthread_mutex_lock(&queue_mutex);
    int customer_id = -1;
    if (queue->size > 0) {
        customer_id = queue->customer_ids[queue->front];
        queue->front = (queue->front + 1) % MAX_QUEUE_SIZE;
        queue->size--;
    }
    pthread_mutex_unlock(&queue_mutex);
    return customer_id;
}

// Initialize the bakery
void init_bakery() {
    srand(time(NULL));
    
    // Initialize semaphores
    sem_init(&red_sem, 0, 0);
    sem_init(&blue_sem, 0, 0);
    sem_init(&tables_sem, 0, NUM_TABLES);
    
    // Initialize customer array
    for (int i = 0; i < MAX_CUSTOMERS; i++) {
        customers[i].id = i;
        customers[i].in_bakery = false;
        customers[i].at_table = false;
        customers[i].table_num = -1;
        customers[i].label = NULL;
        customers[i].customer_widget = NULL;
    }
}

// Clean up resources
void cleanup_bakery() {
    sem_destroy(&red_sem);
    sem_destroy(&blue_sem);
    sem_destroy(&tables_sem);
    pthread_mutex_destroy(&bakery_mutex);
    pthread_mutex_destroy(&queue_mutex);
}

// Apply CSS to a widget
void apply_css(GtkWidget *widget, const char *class_name) {
    GtkStyleContext *context = gtk_widget_get_style_context(widget);
    gtk_style_context_add_class(context, class_name);
}

// Create a colored label for customers
GtkWidget *create_colored_label(const char *text, const char *color_class) {
    GtkWidget *label = gtk_label_new(text);
    gtk_widget_set_size_request(label, 60, 40);
    apply_css(label, "customer-label");
    apply_css(label, color_class);
    return label;
}

// Update the UI with current bakery state (called by GTK main thread)
gboolean update_ui(gpointer data) {
    char buffer[100];
    
    // Update counts
    sprintf(buffer, "Red Customers: %d", red_count);
    gtk_label_set_text(GTK_LABEL(red_count_label), buffer);
    apply_css(red_count_label, "red-text");
    
    sprintf(buffer, "Blue Customers: %d", blue_count);
    gtk_label_set_text(GTK_LABEL(blue_count_label), buffer);
    apply_css(blue_count_label, "blue-text");
    
    sprintf(buffer, "Tables Used: %d/%d", tables_used, NUM_TABLES);
    gtk_label_set_text(GTK_LABEL(tables_label), buffer);
    
    // Update queue counts
    pthread_mutex_lock(&queue_mutex);
    sprintf(buffer, "Red Queue: %d", red_queue.size);
    gtk_label_set_text(GTK_LABEL(red_queue_label), buffer);
    apply_css(red_queue_label, "red-text");
    
    sprintf(buffer, "Blue Queue: %d", blue_queue.size);
    gtk_label_set_text(GTK_LABEL(blue_queue_label), buffer);
    apply_css(blue_queue_label, "blue-text");
    pthread_mutex_unlock(&queue_mutex);
    
    return G_SOURCE_CONTINUE;
}

// Generate a new customer
void create_customer(CustomerColor color) {
    pthread_t customer_thread_id;
    
    pthread_mutex_lock(&bakery_mutex);
    int id = -1;
    for (int i = 0; i < MAX_CUSTOMERS; i++) {
        if (!customers[i].in_bakery && customers[i].customer_widget == NULL) {
            id = i;
            break;
        }
    }
    
    if (id != -1) {
        customers[id].color = color;
        customers[id].in_bakery = false;
        customers[id].at_table = false;
        
        // Create the customer thread
        pthread_create(&customer_thread_id, NULL, customer_thread, &customers[id]);
        pthread_detach(customer_thread_id);
        
        customer_count++;
    }
    pthread_mutex_unlock(&bakery_mutex);
}

// Create a visual representation of a customer
void create_customer_widget(Customer *customer) {
    // Use gdk_threads_add_idle to safely update UI from another thread
    gpointer data = (gpointer)customer;
    gdk_threads_add_idle(G_SOURCE_FUNC(create_customer_widget_idle), data);
}

// Idle function to create customer widget (runs in GTK main thread)
gboolean create_customer_widget_idle(gpointer data) {
    Customer *customer = (Customer *)data;
    
    char label_text[10];
    sprintf(label_text, "%d", customer->id);
    
    if (customer->color == RED) {
        customer->customer_widget = create_colored_label(label_text, "red-customer");
    } else {
        customer->customer_widget = create_colored_label(label_text, "blue-customer");
    }
    
    gtk_widget_show(customer->customer_widget);
    
    // If in the bakery, place at a table, otherwise in the queue
    if (customer->in_bakery && customer->at_table) {
        gtk_grid_attach(GTK_GRID(bakery_grid), customer->customer_widget, 
                       customer->table_num % 3, customer->table_num / 3, 1, 1);
    } else {
        gtk_box_pack_start(GTK_BOX(queue_box), customer->customer_widget, FALSE, FALSE, 5);
    }
    
    return G_SOURCE_REMOVE;  // Remove this idle source
}

// Data structure for customer movement
typedef struct {
    Customer *customer;
    int table_num;
} MoveCustomerData;

// Move a customer from queue to bakery
void move_customer_to_bakery(Customer *customer, int table_num) {
    // Create data for idle function
    MoveCustomerData *data = g_new(MoveCustomerData, 1);
    data->customer = customer;
    data->table_num = table_num;
    
    // Schedule UI update in main thread
    gdk_threads_add_idle(G_SOURCE_FUNC(move_customer_to_bakery_idle), data);
}

// Idle function to move customer (runs in GTK main thread)
gboolean move_customer_to_bakery_idle(gpointer data) {
    MoveCustomerData *move_data = (MoveCustomerData *)data;
    Customer *customer = move_data->customer;
    int table_num = move_data->table_num;
    
    // Remove from queue box
    if (customer->customer_widget != NULL) {
        g_object_ref(customer->customer_widget); // Prevent widget destruction
        gtk_container_remove(GTK_CONTAINER(queue_box), customer->customer_widget);
    } else {
        // Create the widget if it doesn't exist
        char label_text[10];
        sprintf(label_text, "%d", customer->id);
        
        if (customer->color == RED) {
            customer->customer_widget = create_colored_label(label_text, "red-customer");
        } else {
            customer->customer_widget = create_colored_label(label_text, "blue-customer");
        }
        
        gtk_widget_show(customer->customer_widget);
        g_object_ref(customer->customer_widget);
    }
    
    // Add to bakery grid
    gtk_grid_attach(GTK_GRID(bakery_grid), customer->customer_widget, 
                   table_num % 3, table_num / 3, 1, 1);
    
    // Free data struct
    g_free(move_data);
    
    return G_SOURCE_REMOVE;  // Remove this idle source
}

// Remove a customer from the bakery
void remove_customer_from_bakery(Customer *customer) {
    // Schedule UI update in main thread
    gdk_threads_add_idle(G_SOURCE_FUNC(remove_customer_from_bakery_idle), customer);
}

// Idle function to remove customer (runs in GTK main thread)
gboolean remove_customer_from_bakery_idle(gpointer data) {
    Customer *customer = (Customer *)data;
    
    if (customer->customer_widget != NULL) {
        // Remove from container (grid or box)
        GtkWidget *parent = gtk_widget_get_parent(customer->customer_widget);
        if (parent != NULL) {
            g_object_ref(customer->customer_widget); // Prevent widget destruction
            gtk_container_remove(GTK_CONTAINER(parent), customer->customer_widget);
        }
        
        // Destroy the widget
        gtk_widget_destroy(customer->customer_widget);
        customer->customer_widget = NULL;
    }
    
    return G_SOURCE_REMOVE;  // Remove this idle source
}

// Customer thread function
void *customer_thread(void *arg) {
    Customer *customer = (Customer *)arg;
    CustomerColor color = customer->color;
    
    // Add customer to appropriate queue
    if (color == RED) {
        enqueue(&red_queue, customer->id);
    } else {
        enqueue(&blue_queue, customer->id);
    }
    
    // Create visual representation
    create_customer_widget(customer);
    
    // Try to enter bakery based on the equal number rule
    pthread_mutex_lock(&bakery_mutex);
    
    if (color == RED) {
        if (red_count <= blue_count) {
            // Red customer can enter directly
            red_count++;
            customer->in_bakery = true;
            dequeue(&red_queue); // Remove from queue
            pthread_mutex_unlock(&bakery_mutex);
        } else {
            pthread_mutex_unlock(&bakery_mutex);
            // Wait for signal to enter
            sem_wait(&red_sem);
            
            pthread_mutex_lock(&bakery_mutex);
            red_count++;
            customer->in_bakery = true;
            pthread_mutex_unlock(&bakery_mutex);
        }
    } else { // BLUE
        if (blue_count <= red_count) {
            // Blue customer can enter directly
            blue_count++;
            customer->in_bakery = true;
            dequeue(&blue_queue); // Remove from queue
            pthread_mutex_unlock(&bakery_mutex);
        } else {
            pthread_mutex_unlock(&bakery_mutex);
            // Wait for signal to enter
            sem_wait(&blue_sem);
            
            pthread_mutex_lock(&bakery_mutex);
            blue_count++;
            customer->in_bakery = true;
            pthread_mutex_unlock(&bakery_mutex);
        }
    }
    
    // Wait for an available table
    sem_wait(&tables_sem);
    
    // Find an available table
    pthread_mutex_lock(&bakery_mutex);
    int table_num = -1;
    for (int i = 0; i < NUM_TABLES; i++) {
        bool table_occupied = false;
        
        for (int j = 0; j < MAX_CUSTOMERS; j++) {
            if (customers[j].at_table && customers[j].table_num == i) {
                table_occupied = true;
                break;
            }
        }
        
        if (!table_occupied) {
            table_num = i;
            break;
        }
    }
    
    if (table_num != -1) {
        customer->at_table = true;
        customer->table_num = table_num;
        tables_used++;
        
        // Move customer to bakery visually
        move_customer_to_bakery(customer, table_num);
    }
    pthread_mutex_unlock(&bakery_mutex);
    
    // Customer stays at the bakery for a random amount of time
    int stay_time = CUSTOMER_STAY_MIN + (rand() % (CUSTOMER_STAY_MAX - CUSTOMER_STAY_MIN + 1));
    sleep(stay_time);
    
    // Customer leaves
    pthread_mutex_lock(&bakery_mutex);
    
    if (customer->at_table) {
        tables_used--;
        customer->at_table = false;
    }
    
    if (customer->color == RED) {
        red_count--;
        // Signal a waiting blue customer to enter
        if (blue_queue.size > 0) {
            sem_post(&blue_sem);
        }
    } else {
        blue_count--;
        // Signal a waiting red customer to enter
        if (red_queue.size > 0) {
            sem_post(&red_sem);
        }
    }
    
    customer->in_bakery = false;
    remove_customer_from_bakery(customer);
    pthread_mutex_unlock(&bakery_mutex);
    
    // Release table for next customer
    sem_post(&tables_sem);
    
    return NULL;
}

// Function to generate new customers periodically
gboolean generate_customer(gpointer data) {
    if (!running) return G_SOURCE_REMOVE;
    
    CustomerColor color = (rand() % 2 == 0) ? RED : BLUE;
    create_customer(color);
    
    return G_SOURCE_CONTINUE;
}

// Setup the CSS for the UI
void setup_css() {
    provider = gtk_css_provider_new();
    
    const char *css = 
        ".customer-label {"
        "  border-radius: 5px;"
        "  font-weight: bold;"
        "  color: white;"
        "  padding: 5px;"
        "  margin: 2px;"
        "  text-shadow: 1px 1px 1px rgba(0,0,0,0.5);"
        "}"
        ".red-customer {"
        "  background-color: #FF5555;"
        "  border: 2px solid #CC0000;"
        "}"
        ".blue-customer {"
        "  background-color: #5555FF;"
        "  border: 2px solid #0000CC;"
        "}"
        ".red-text {"
        "  color: #FF0000;"
        "  font-weight: bold;"
        "}"
        ".blue-text {"
        "  color: #0000FF;"
        "  font-weight: bold;"
        "}"
        ".bakery-grid {"
        "  background-color: #FFEECC;"
        "  border: 3px solid #BB9966;"
        "  border-radius: 10px;"
        "  padding: 10px;"
        "}"
        ".queue-box {"
        "  background-color: #CCCCFF;"
        "  border: 3px solid #9999CC;"
        "  border-radius: 10px;"
        "  padding: 10px;"
        "}"
        ".header-label {"
        "  font-size: 16px;"
        "  font-weight: bold;"
        "  margin: 5px;"
        "  color: #663300;"
        "}"
        ".status-label {"
        "  font-style: italic;"
        "  color: #666666;"
        "}";
    
    gtk_css_provider_load_from_data(provider, css, -1, NULL);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
}

// Create and setup the UI
void create_ui() {
    // Create window
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Sweet Harmony Bakery Simulation");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    gtk_container_set_border_width(GTK_CONTAINER(window), 10);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    
    // Apply CSS
    setup_css();
    
    // Main container
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(window), main_box);
    
    // Title
    GtkWidget *title_label = gtk_label_new("Sweet Harmony Bakery");
    PangoAttrList *attr_list = pango_attr_list_new();
    PangoAttribute *attr = pango_attr_size_new(24 * PANGO_SCALE);
    pango_attr_list_insert(attr_list, attr);
    attr = pango_attr_weight_new(PANGO_WEIGHT_BOLD);
    pango_attr_list_insert(attr_list, attr);
    gtk_label_set_attributes(GTK_LABEL(title_label), attr_list);
    pango_attr_list_unref(attr_list);
    gtk_box_pack_start(GTK_BOX(main_box), title_label, FALSE, FALSE, 10);
    
    // Info box (for counts)
    GtkWidget *info_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
    gtk_box_pack_start(GTK_BOX(main_box), info_box, FALSE, FALSE, 5);
    
    // Customer counts
    red_count_label = gtk_label_new("Red Customers: 0");
    blue_count_label = gtk_label_new("Blue Customers: 0");
    tables_label = gtk_label_new("Tables Used: 0/5");
    
    gtk_box_pack_start(GTK_BOX(info_box), red_count_label, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(info_box), blue_count_label, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(info_box), tables_label, FALSE, FALSE, 5);
    
    apply_css(red_count_label, "red-text");
    apply_css(blue_count_label, "blue-text");
    
    // Queue info
    GtkWidget *queue_info_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
    gtk_box_pack_start(GTK_BOX(main_box), queue_info_box, FALSE, FALSE, 5);
    
    red_queue_label = gtk_label_new("Red Queue: 0");
    blue_queue_label = gtk_label_new("Blue Queue: 0");
    
    gtk_box_pack_start(GTK_BOX(queue_info_box), red_queue_label, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(queue_info_box), blue_queue_label, FALSE, FALSE, 5);
    
    apply_css(red_queue_label, "red-text");
    apply_css(blue_queue_label, "blue-text");
    
    // Content area (bakery and queue)
    GtkWidget *content_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(main_box), content_box, TRUE, TRUE, 5);
    
    // Bakery area
    GtkWidget *bakery_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(content_box), bakery_box, TRUE, TRUE, 5);
    
    GtkWidget *bakery_label = gtk_label_new("Bakery Tables");
    apply_css(bakery_label, "header-label");
    gtk_box_pack_start(GTK_BOX(bakery_box), bakery_label, FALSE, FALSE, 5);
    
    // Bakery grid (tables arrangement)
    bakery_grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(bakery_grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(bakery_grid), 10);
    gtk_widget_set_halign(bakery_grid, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(bakery_grid, GTK_ALIGN_CENTER);
    apply_css(bakery_grid, "bakery-grid");
    
    // Create table placeholders
    for (int i = 0; i < NUM_TABLES; i++) {
        GtkWidget *table_label = gtk_label_new("Table");
        gtk_grid_attach(GTK_GRID(bakery_grid), table_label, i % 3, i / 3, 1, 1);
    }
    
    GtkWidget *bakery_frame = gtk_frame_new(NULL);
    gtk_container_add(GTK_CONTAINER(bakery_frame), bakery_grid);
    gtk_box_pack_start(GTK_BOX(bakery_box), bakery_frame, TRUE, TRUE, 5);
    
    // Queue area
    GtkWidget *queue_area = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(content_box), queue_area, TRUE, TRUE, 5);
    
    GtkWidget *queue_label = gtk_label_new("Waiting Queue");
    apply_css(queue_label, "header-label");
    gtk_box_pack_start(GTK_BOX(queue_area), queue_label, FALSE, FALSE, 5);
    
    // Queue box (waiting customers)
    GtkWidget *queue_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(queue_scroll),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    
    queue_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    apply_css(queue_box, "queue-box");
    
    gtk_container_add(GTK_CONTAINER(queue_scroll), queue_box);
    gtk_box_pack_start(GTK_BOX(queue_area), queue_scroll, TRUE, TRUE, 5);
    
    // Status label
    status_label = gtk_label_new("Bakery is open! Customers are arriving...");
    apply_css(status_label, "status-label");
    gtk_box_pack_start(GTK_BOX(main_box), status_label, FALSE, FALSE, 5);
    
    // Control buttons
    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(main_box), button_box, FALSE, FALSE, 5);
    
    GtkWidget *add_red_button = gtk_button_new_with_label("Add Red Customer");
    GtkWidget *add_blue_button = gtk_button_new_with_label("Add Blue Customer");
    GtkWidget *exit_button = gtk_button_new_with_label("Exit");
    
    g_signal_connect(add_red_button, "clicked", G_CALLBACK(on_add_red_clicked), NULL);
    g_signal_connect(add_blue_button, "clicked", G_CALLBACK(on_add_blue_clicked), NULL);
    g_signal_connect(exit_button, "clicked", G_CALLBACK(gtk_main_quit), NULL);
    
    gtk_box_pack_start(GTK_BOX(button_box), add_red_button, TRUE, TRUE, 5);
    gtk_box_pack_start(GTK_BOX(button_box), add_blue_button, TRUE, TRUE, 5);
    gtk_box_pack_start(GTK_BOX(button_box), exit_button, TRUE, TRUE, 5);
    
    // Show all widgets
    gtk_widget_show_all(window);
}

// Button handlers
void on_add_red_clicked(GtkWidget *widget, gpointer data) {
    create_customer(RED);
}

void on_add_blue_clicked(GtkWidget *widget, gpointer data) {
    create_customer(BLUE);
}

int main(int argc, char *argv[]) {
    // Initialize GTK
    gtk_init(&argc, &argv);
    
    // Initialize bakery
    init_bakery();
    
    // Create the UI
    create_ui();
    
    // Set up timers for UI updates and customer generation
    g_timeout_add(500, update_ui, NULL);
    g_timeout_add_seconds(NEW_CUSTOMER_INTERVAL, generate_customer, NULL);
    
    // Start GTK main loop
    gtk_main();
    
    // Clean up
    running = false;
    cleanup_bakery();
    
    return 0;
}
