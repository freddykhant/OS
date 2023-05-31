#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define MAX_CUSTOMERS 100

// Structure to represent a customer
typedef struct
{
    int number;
    char type;
    char* arrival;
    int teller;
} Customer;

// Global variables
Customer c_queue[MAX_CUSTOMERS];    // Initialize queue
int c_head = 0;                     // Front of queue        
int c_tail = -1;                    // Back of queue
int c_count = 0;                    // Queue count

int m;                              // Size of customer queue
int tw, td, ti;                     // Service duration for withdrawal, deposit, and information

pthread_mutex_t queue_mutex;        // Mutex for accessing the queue
pthread_mutex_t log_mutex;          // Mutex for accessing the log file
pthread_cond_t queue_cond;          // Condition variable to signal when the queue is not empty

int served_customers[4] = {0};      // Array to track number of customers served by each teller
int all_joined = 0;                 // Variable to check whether all customers have joined the queue
int all_served = 0;                 // Variable to check whether all customers have been served
int total_customers = 0;            // Total customers that have joined
int served_customers_count = 0;     // Total customers served by all tellers

FILE *c_file;
FILE *r_log;

char* get_time()
{
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char* time_string = malloc(sizeof(char) * 9);  // HH:MM:SS + null terminator
    strftime(time_string, 9, "%T", tm_info);
    return time_string;
}

// Function to add a customer to the queue
void enqueue(Customer customer)
{
    pthread_mutex_lock(&queue_mutex);
    while (c_count >= m)
    {
        pthread_cond_wait(&queue_cond, &queue_mutex);
    }
    c_tail = (c_tail + 1) % MAX_CUSTOMERS;
    c_queue[c_tail] = customer;
    c_count++;
    pthread_mutex_unlock(&queue_mutex);
    pthread_cond_signal(&queue_cond);
}

// Function to remove a customer from the queue
Customer dequeue()
{
    pthread_mutex_lock(&queue_mutex);
    while (c_count <= 0) 
    {
        pthread_cond_wait(&queue_cond, &queue_mutex);
    }
    Customer customer = c_queue[c_head];
    c_head = (c_head + 1) % MAX_CUSTOMERS;
    c_count--;
    pthread_mutex_unlock(&queue_mutex);
    pthread_cond_signal(&queue_cond);
    return customer;
}

// Function to periodically get a customer from c_file and put them in c_queue
void* customer(void* arg)
{
    int tc = *(int*)arg;
    c_file = fopen("c_file", "r");
    char line[256];
    int customer_num;
    char service_type;

    if(c_file == NULL)
    {
        fprintf(stderr, "Error opening file\n");
        exit(EXIT_FAILURE);
    }
    // Read customer details from c_file
    while(fgets(line, sizeof(line), c_file))
    {
        // Read contents
        sscanf(line, "%d %c", &customer_num, &service_type);

        // Create a new Customer
        Customer customer;
        customer.number = customer_num;
        customer.type = service_type;
        customer.arrival = get_time();
        customer.teller = 0;

        // Add customer to queue
        enqueue(customer);
        total_customers++;

        // Log the customer arrival
        pthread_mutex_lock(&log_mutex);
        char* time_string = get_time();
        r_log = fopen("r_log", "a");
        fprintf(r_log, "-----------------------------------------------------------------------\n");
        fprintf(r_log, "Customer#: %d\n", customer.number);
        fprintf(r_log, "Service type: %c\n", customer.type);
        fprintf(r_log, "Arrival time: %s\n", time_string);
        fprintf(r_log, "-----------------------------------------------------------------------\n\n");
        pthread_mutex_unlock(&log_mutex);
        fclose(r_log);

        // Sleep for tc seconds before getting the next customer
        sleep(tc);
    }
    fclose(c_file);
    return NULL;
}

// Function to remove a customer from the customer queue and process them
void* teller(void* arg)
{
    int teller_no = *(int*)arg;
    char* start = get_time();

    while (!all_joined && !all_served)
    {
       // Get the customer from the queue
        Customer customer = dequeue();
        customer.teller = teller_no;

        // Time taken to complete service
        int service_time = 0;
        if(customer.type == 'W'){
            service_time = tw;
        }
        else if(customer.type == 'D'){
            service_time = td;
        }
        else if(customer.type == 'I'){
            service_time = ti;
        }

        // Log the customer response time
        pthread_mutex_lock(&log_mutex);
        char* response = get_time();
        r_log = fopen("r_log", "a");
        fprintf(r_log, "Teller: %d\n", teller_no);
        fprintf(r_log, "Customer: %d\n", customer.number);
        fprintf(r_log, "Arrival time: %s\n", customer.arrival);
        fprintf(r_log, "Response time: %s\n\n", response);
        fclose(r_log);
        pthread_mutex_unlock(&log_mutex);

        // Serve customer
        sleep(service_time);

        // Log the customer completion time
        pthread_mutex_lock(&log_mutex);
        char* completion = get_time();
        r_log = fopen("r_log", "a");
        fprintf(r_log, "Teller: %d\n", teller_no);
        fprintf(r_log, "Customer: %d\n", customer.number);
        fprintf(r_log, "Arrival time: %s\n", customer.arrival);
        fprintf(r_log, "Completion time: %s\n\n", completion);
        fclose(r_log);
        pthread_mutex_unlock(&log_mutex);
        
        // Update the number of served customers for the teller
        served_customers[teller_no-1]++;
        served_customers_count++;

        // Terminate once all customers are served
        if(served_customers_count >= total_customers){
            all_served = 1;
        }
    }
    pthread_mutex_lock(&log_mutex);
    char* termination = get_time();
    r_log = fopen("r_log", "a");
    fprintf(r_log, "Termination: teller-%d\n", teller_no);
    fprintf(r_log, "#served customers: %d\n", served_customers[teller_no-1]);
    fprintf(r_log, "Start time: %s\n", start);
    fprintf(r_log, "Completion time: %s\n\n", termination);
    fclose(r_log);
    pthread_mutex_unlock(&log_mutex);
    
    return NULL;
}

int main(int argc, char *argv[])
{
    // Check for correct number of arguments
    if(argc != 6)
    {
        printf("Usage: %s cq m tc tw td ti\n", argv[0]);
        printf("cq: Executable program name\n");
        printf("m: Size of customer queue\n");
        printf("tc: Periodic time for customer arrival\n");
        printf("tw: Time duration for withdrawal service\n");
        printf("td: Time duration for deposit service\n");
        printf("ti: Time duration for information service\n");
        return 1;
    }
    // Check arguments are positive integers
    for (int i = 1; i <= 5; i++)
    {
        if (atoi(argv[i]) <= 0)
        {
            printf("Arguments must be positive integers\n");
            return 1;
        }
    }
    // Parse command line arguments
    m = atoi(argv[1]);
    int tc = atoi(argv[2]);
    tw = atoi(argv[3]);
    td = atoi(argv[4]);
    ti = atoi(argv[5]);

    // Initialize mutex and condition variable
    pthread_mutex_init(&queue_mutex, NULL);
    pthread_mutex_init(&log_mutex, NULL);
    pthread_cond_init(&queue_cond, NULL);

    // Create customer thread
    pthread_t customer_thread;
    pthread_create(&customer_thread, NULL, customer, &tc);

    // Create teller threads
    pthread_t teller_threads[4];
    for (int i = 0; i < 4; i++)
    {
        int *teller_no = malloc(sizeof(int));
        *teller_no = i + 1;
        pthread_create(&teller_threads[i], NULL, teller, teller_no);
    }

    // Wait for the customer thread to join
    pthread_join(customer_thread, NULL);

    // Terminate once all customers have joined queue
    all_joined = 1;

    // Wait for teller threads to join
    for (int i = 0; i < 4; i++)
    {
        pthread_join(teller_threads[i], NULL);
    }

    // Print the teller statistics
    int total_served = 0;
    pthread_mutex_lock(&log_mutex);
    r_log = fopen("r_log", "a");
    fprintf(r_log, "Teller Statistics\n");
    for (int i = 0; i < 4; i++)
    {
        fprintf(r_log, "Teller-%d serves %d customers.\n", i + 1, served_customers[i]);
        total_served += served_customers[i];
    }
    fprintf(r_log, "Total number of customers: %d\n", total_served);
    fclose(r_log);
    pthread_mutex_unlock(&log_mutex);

    // Destroy mutex and condition variable
    pthread_mutex_destroy(&queue_mutex);
    pthread_mutex_destroy(&log_mutex);
    pthread_cond_destroy(&queue_cond);

    return 0;
}