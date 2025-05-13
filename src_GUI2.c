#include <gtk/gtk.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>

// Configuration will be set by user input
int NUM_TABLES;
int MAX_CUSTOMERS;

typedef enum { RED, BLUE } CustomerColor;

typedef struct {
    int id;
    CustomerColor color;
    bool in_bakery;
    int table_num;
    GtkWidget *widget;
} Customer;

// Global Variables
sem_t tables_sem;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int red_count = 0;
int blue_count = 0;
int tables_used = 0;
bool running = true;

Customer *customers;

// GTK Widgets
GtkWidget *window;
GtkWidget *red_count_label;
GtkWidget *blue_count_label;
GtkWidget *tables_label;
GtkWidget *bakery_grid;
GtkWidget *queue_box;
GtkWidget *status_label;
GtkCssProvider *provider;
GtkWidget **table_widgets;

// Forward declarations
void create_customer(CustomerColor color);
void create_ui(void);
gboolean update_ui(gpointer data);

// Callback functions
void on_add_red_clicked(GtkWidget *widget, gpointer data) {
    create_customer(RED);
}

void on_add_blue_clicked(GtkWidget *widget, gpointer data) {
    create_customer(BLUE);
}

void apply_css(GtkWidget *widget, const char *class_name) {
    GtkStyleContext *context = gtk_widget_get_style_context(widget);
    gtk_style_context_add_class(context, class_name);
}

void setup_css() {
    provider = gtk_css_provider_new();
    const char *css = 
        ".customer-label {"
        "  border-radius: 5px;"
        "  font-weight: bold;"
        "  color: white;"
        "  padding: 5px;"
        "  margin: 2px;"
        "}"
        ".red-customer {"
        "  background-color: #FF5555;"
        "  border: 2px solid #CC0000;"
        "}"
        ".blue-customer {"
        "  background-color: #5555FF;"
        "  border: 2px solid #0000CC;"
        "}"
        ".bakery-grid {"
        "  background-color: #FFEECC;"
        "  border: 3px solid #BB9966;"
        "  padding: 10px;"
        "}"
        ".queue-box {"
        "  background-color: #CCCCFF;"
        "  border: 2px solid #9999CC;"
        "  padding: 10px;"
        "  min-height: 100px;"
        "}"
        ".table-empty {"
        "  background-color: #FFFFFF;"
        "  border: 2px solid #666666;"
        "  padding: 10px;"
        "  min-width: 100px;"
        "  min-height: 80px;"
        "}";
    
    gtk_css_provider_load_from_data(provider, css, -1, NULL);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
}

bool is_table_occupied(int table_num) {
    for (int i = 0; i < MAX_CUSTOMERS; i++) {
        if (customers[i].in_bakery && customers[i].table_num == table_num) {
            return true;
        }
    }
    return false;
}
void create_customer_widget(Customer *customer) {
    char label_text[10];
    sprintf(label_text, "C%d", customer->id);
    
    customer->widget = gtk_label_new(label_text);
    gtk_widget_set_size_request(customer->widget, 60, 40);
    
    const char *color_class = (customer->color == RED) ? "red-customer" : "blue-customer";
    apply_css(customer->widget, "customer-label");
    apply_css(customer->widget, color_class);
    
    gtk_widget_show(customer->widget);
    gtk_box_pack_start(GTK_BOX(queue_box), customer->widget, FALSE, FALSE, 5);
}

void move_to_table(Customer *customer) {
    if (customer->widget && customer->table_num >= 0) {
        g_object_ref(customer->widget);
        gtk_container_remove(GTK_CONTAINER(gtk_widget_get_parent(customer->widget)), 
                           customer->widget);
        
        // Remove existing children from the table container
        GList *children = gtk_container_get_children(GTK_CONTAINER(table_widgets[customer->table_num]));
        for (GList *iter = children; iter != NULL; iter = g_list_next(iter)) {
            gtk_container_remove(GTK_CONTAINER(table_widgets[customer->table_num]), 
                               GTK_WIDGET(iter->data));
        }
        g_list_free(children);
        
        // Add customer widget to table
        gtk_container_add(GTK_CONTAINER(table_widgets[customer->table_num]), customer->widget);
        gtk_widget_show_all(table_widgets[customer->table_num]);
    }
}

void remove_customer(Customer *customer) {
    if (customer->widget) {
        gtk_widget_destroy(customer->widget);
        customer->widget = NULL;
    }
    
    if (customer->color == RED) red_count--;
    else blue_count--;
    
    if (customer->table_num >= 0) {
        tables_used--;
        customer->table_num = -1;
    }
    
    customer->in_bakery = false;
}

void *customer_thread(void *arg) {
    Customer *customer = (Customer *)arg;
    
    pthread_mutex_lock(&mutex);
    if (customer->color == RED) {
        red_count++;
    } else {
        blue_count++;
    }
    create_customer_widget(customer);
    pthread_mutex_unlock(&mutex);
    
    sem_wait(&tables_sem);
    
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < NUM_TABLES; i++) {
        if (!is_table_occupied(i)) {
            customer->table_num = i;
            customer->in_bakery = true;
            tables_used++;
            move_to_table(customer);
            break;
        }
    }
    pthread_mutex_unlock(&mutex);
    
    sleep(3 + (rand() % 5));
    
    pthread_mutex_lock(&mutex);
    remove_customer(customer);
    pthread_mutex_unlock(&mutex);
    
    sem_post(&tables_sem);
    return NULL;
}

void create_customer(CustomerColor color) {
    pthread_t thread;
    int id = -1;
    
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < MAX_CUSTOMERS; i++) {
        if (!customers[i].in_bakery && customers[i].widget == NULL) {
            id = i;
            break;
        }
    }
    
    if (id != -1) {
        customers[id].color = color;
        customers[id].in_bakery = false;
        customers[id].table_num = -1;
        pthread_create(&thread, NULL, customer_thread, &customers[id]);
        pthread_detach(thread);
    }
    pthread_mutex_unlock(&mutex);
}
void create_ui() {
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Bakery Simulation");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    
    setup_css();
    
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(window), main_box);
    
    // Title
    GtkWidget *title = gtk_label_new("Bakery Simulation");
    PangoAttrList *attr_list = pango_attr_list_new();
    PangoAttribute *attr = pango_attr_size_new(24 * PANGO_SCALE);
    pango_attr_list_insert(attr_list, attr);
    gtk_label_set_attributes(GTK_LABEL(title), attr_list);
    pango_attr_list_unref(attr_list);
    gtk_box_pack_start(GTK_BOX(main_box), title, FALSE, FALSE, 10);

    // Info labels
    GtkWidget *info_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    red_count_label = gtk_label_new("Red Customers: 0");
    blue_count_label = gtk_label_new("Blue Customers: 0");
    tables_label = gtk_label_new("Tables Used: 0/0");
    
    gtk_box_pack_start(GTK_BOX(info_box), red_count_label, TRUE, TRUE, 5);
    gtk_box_pack_start(GTK_BOX(info_box), blue_count_label, TRUE, TRUE, 5);
    gtk_box_pack_start(GTK_BOX(info_box), tables_label, TRUE, TRUE, 5);
    gtk_box_pack_start(GTK_BOX(main_box), info_box, FALSE, FALSE, 5);
    
    // Tables area
    GtkWidget *tables_label_header = gtk_label_new("Tables");
    gtk_box_pack_start(GTK_BOX(main_box), tables_label_header, FALSE, FALSE, 5);
    
    GtkWidget *tables_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(tables_scroll),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(main_box), tables_scroll, TRUE, TRUE, 5);
    
    bakery_grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(bakery_grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(bakery_grid), 10);
    gtk_container_add(GTK_CONTAINER(tables_scroll), bakery_grid);
    apply_css(bakery_grid, "bakery-grid");
    
    // Create table widgets
    table_widgets = g_new(GtkWidget*, NUM_TABLES);
    for (int i = 0; i < NUM_TABLES; i++) {
        table_widgets[i] = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
        apply_css(table_widgets[i], "table-empty");
        
        char table_label[20];
        sprintf(table_label, "Table %d", i + 1);
        GtkWidget *label = gtk_label_new(table_label);
        gtk_container_add(GTK_CONTAINER(table_widgets[i]), label);
        
        gtk_grid_attach(GTK_GRID(bakery_grid), table_widgets[i],
                       i % 3, i / 3, 1, 1);
    }
    
    // Queue area
    GtkWidget *queue_label = gtk_label_new("Waiting Queue");
    gtk_box_pack_start(GTK_BOX(main_box), queue_label, FALSE, FALSE, 5);
    
    queue_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    apply_css(queue_box, "queue-box");
    gtk_box_pack_start(GTK_BOX(main_box), queue_box, FALSE, FALSE, 5);
    
    // Control buttons
    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget *red_button = gtk_button_new_with_label("Add Red Customer");
    GtkWidget *blue_button = gtk_button_new_with_label("Add Blue Customer");
    
    g_signal_connect(red_button, "clicked", G_CALLBACK(on_add_red_clicked), NULL);
    g_signal_connect(blue_button, "clicked", G_CALLBACK(on_add_blue_clicked), NULL);
    
    gtk_box_pack_start(GTK_BOX(button_box), red_button, TRUE, TRUE, 5);
    gtk_box_pack_start(GTK_BOX(button_box), blue_button, TRUE, TRUE, 5);
    gtk_box_pack_start(GTK_BOX(main_box), button_box, FALSE, FALSE, 5);
    
    gtk_widget_show_all(window);
}

gboolean update_ui(gpointer data) {
    char buffer[32];
    
    sprintf(buffer, "Red Customers: %d", red_count);
    gtk_label_set_text(GTK_LABEL(red_count_label), buffer);
    
    sprintf(buffer, "Blue Customers: %d", blue_count);
    gtk_label_set_text(GTK_LABEL(blue_count_label), buffer);
    
    sprintf(buffer, "Tables Used: %d/%d", tables_used, NUM_TABLES);
    gtk_label_set_text(GTK_LABEL(tables_label), buffer);
    
    return G_SOURCE_CONTINUE;
}

int main(int argc, char *argv[]) {
    printf("Enter number of tables: ");
    scanf("%d", &NUM_TABLES);
    
    printf("Enter maximum number of customers: ");
    scanf("%d", &MAX_CUSTOMERS);
    
    gtk_init(&argc, &argv);
    
    // Allocate memory for customers array
    customers = (Customer*)malloc(MAX_CUSTOMERS * sizeof(Customer));
    for (int i = 0; i < MAX_CUSTOMERS; i++) {
        customers[i].id = i;
        customers[i].in_bakery = false;
        customers[i].table_num = -1;
        customers[i].widget = NULL;
    }
    
    sem_init(&tables_sem, 0, NUM_TABLES);
    srand(time(NULL));
    
    create_ui();
    g_timeout_add(500, update_ui, NULL);
    
    gtk_main();
    
    // Cleanup
    sem_destroy(&tables_sem);
    pthread_mutex_destroy(&mutex);
    free(customers);
    g_free(table_widgets);
    
    return 0;
}
