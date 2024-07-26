#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <limits.h>
#include <sys/time.h>

#define TIME_SLICE 50 // Time slice in milliseconds

const int TASK_COUNT = 10;
const int TEST_COUNT = 20;

const int MINIMUM_BURST_TIME = 25;   // in milliseconds
const int MAXIMUM_BURST_TIME = 1000; // in milliseconds
const int MINIMUM_IO_WAIT = 250;     // in milliseconds
const int MAXIMUM_IO_WAIT = 1000;    // in milliseconds
const int MINIMUM_PRIORITY = 1;
const int MAXIMUM_PRIORITY = 5;

pthread_mutex_t mutex; // Mutex for task synchronization

typedef struct
{
    int id;
    int burst_time;
    int remaining_time;
    int io_time;
    int priority;
    int completed;
    int waiting_for_io;
} task_t;

void fifo_scheduling(task_t *tasks, int num_tasks);
void rr_scheduling(task_t *tasks, int num_tasks);
void sjf_scheduling(task_t *tasks, int num_tasks);
void priority_scheduling(task_t *tasks, int num_tasks);
void generate_random_tasks(task_t *tasks, int num_tasks);
void copy_tasks(task_t *dest, task_t *src, int num_tasks);
void sleep_ms(int milliseconds);
double get_time();
void perform_io(task_t *task);
void *io_task_function(void *arg);

int sjf_compare(const void *a, const void *b)
{
    task_t *task_a = (task_t *)a;
    task_t *task_b = (task_t *)b;
    return task_a->burst_time - task_b->burst_time;
}

int priority_compare(const void *a, const void *b)
{
    task_t *task_a = (task_t *)a;
    task_t *task_b = (task_t *)b;
    return task_a->priority - task_b->priority;
}

int main(int argc, const char *argv[])
{
    srand(time(NULL)); // Seed the random number generator

    FILE *output_file = fopen("scheduling_results.csv", "w");
    if (output_file == NULL)
    {
        fprintf(stderr, "Error opening output file\n");
        return 1;
    }
    fprintf(output_file, "Algorithm,Total Time (s)\n");

    pthread_mutex_init(&mutex, NULL);

    for (int i = 0; i < TEST_COUNT; i++)
    {
        printf("Run %d of %d\n", i + 1, TEST_COUNT);
        task_t *original_tasks = malloc(TASK_COUNT * sizeof(task_t));
        generate_random_tasks(original_tasks, TASK_COUNT);

        double start_time, end_time, total_time;

        // Test FIFO scheduling
        printf("Starting FIFO\n");
        task_t *tasks_fifo = malloc(TASK_COUNT * sizeof(task_t));
        copy_tasks(tasks_fifo, original_tasks, TASK_COUNT);
        start_time = get_time();
        fifo_scheduling(tasks_fifo, TASK_COUNT);
        end_time = get_time();
        total_time = end_time - start_time;
        fprintf(output_file, "FIFO,%f\n", total_time);
        free(tasks_fifo);

        // Test Round Robin scheduling
        printf("Starting RR\n");
        task_t *tasks_rr = malloc(TASK_COUNT * sizeof(task_t));
        copy_tasks(tasks_rr, original_tasks, TASK_COUNT);
        start_time = get_time();
        rr_scheduling(tasks_rr, TASK_COUNT);
        end_time = get_time();
        total_time = end_time - start_time;
        fprintf(output_file, "Round Robin,%f\n", total_time);
        free(tasks_rr);

        // Test SJF scheduling
        printf("Starting SJF\n");
        task_t *tasks_sjf = malloc(TASK_COUNT * sizeof(task_t));
        copy_tasks(tasks_sjf, original_tasks, TASK_COUNT);
        start_time = get_time();
        sjf_scheduling(tasks_sjf, TASK_COUNT);
        end_time = get_time();
        total_time = end_time - start_time;
        fprintf(output_file, "SJF,%f\n", total_time);
        free(tasks_sjf);

        // Test Priority scheduling
        printf("Starting Priority\n");
        task_t *tasks_priority = malloc(TASK_COUNT * sizeof(task_t));
        copy_tasks(tasks_priority, original_tasks, TASK_COUNT);
        start_time = get_time();
        priority_scheduling(tasks_priority, TASK_COUNT);
        end_time = get_time();
        total_time = end_time - start_time;
        fprintf(output_file, "Priority,%f\n", total_time);
        free(tasks_priority);

        free(original_tasks);

        printf("Finished run %d\n", i + 1);
    }

    fclose(output_file);
    pthread_mutex_destroy(&mutex);

    return 0;
}

void fifo_scheduling(task_t *tasks, int num_tasks)
{
    for (int i = 0; i < num_tasks; i++)
    {
        pthread_mutex_lock(&mutex);
        if (tasks[i].completed)
        {
            pthread_mutex_unlock(&mutex);
            continue;
        }
        printf("Task %d running for %d ms\n", tasks[i].id, tasks[i].burst_time);
        sleep_ms(tasks[i].burst_time);
        pthread_mutex_unlock(&mutex);
        perform_io(&tasks[i]);
        tasks[i].completed = 1;
    }
}

void rr_scheduling(task_t *tasks, int num_tasks)
{
    int tasks_remaining = num_tasks;
    while (tasks_remaining > 0)
    {
        for (int i = 0; i < num_tasks; i++)
        {
            pthread_mutex_lock(&mutex);
            if (tasks[i].completed || tasks[i].waiting_for_io)
            {
                pthread_mutex_unlock(&mutex);
                continue;
            }
            int run_time = tasks[i].remaining_time > TIME_SLICE ? TIME_SLICE : tasks[i].remaining_time;
            printf("Task %d running for %d ms\n", tasks[i].id, run_time);
            pthread_mutex_unlock(&mutex);
            sleep_ms(run_time);
            pthread_mutex_lock(&mutex);
            tasks[i].remaining_time -= run_time;
            if (tasks[i].remaining_time <= 0)
            {
                tasks[i].waiting_for_io = 1;
                pthread_mutex_unlock(&mutex);
                perform_io(&tasks[i]);
                pthread_mutex_lock(&mutex);
                tasks[i].completed = 1;
                tasks_remaining--;
            }
            pthread_mutex_unlock(&mutex);
        }
    }
}

void sjf_scheduling(task_t *tasks, int num_tasks)
{
    qsort(tasks, num_tasks, sizeof(task_t), sjf_compare);
    rr_scheduling(tasks, num_tasks); // Using RR to handle I/O during SJF scheduling
}

void priority_scheduling(task_t *tasks, int num_tasks)
{
    qsort(tasks, num_tasks, sizeof(task_t), priority_compare);
    rr_scheduling(tasks, num_tasks); // Using RR to handle I/O during Priority scheduling
}

void generate_random_tasks(task_t *tasks, int num_tasks)
{
    for (int i = 0; i < num_tasks; i++)
    {
        tasks[i].id = i + 1;
        tasks[i].burst_time = (rand() % (MAXIMUM_BURST_TIME - MINIMUM_BURST_TIME + 1) + MINIMUM_BURST_TIME);
        tasks[i].remaining_time = tasks[i].burst_time;
        tasks[i].priority = rand() % (MAXIMUM_PRIORITY - MINIMUM_PRIORITY + 1) + MINIMUM_PRIORITY;
        tasks[i].io_time = (rand() % (MAXIMUM_IO_WAIT - MINIMUM_IO_WAIT + 1) + MINIMUM_IO_WAIT);
        tasks[i].completed = 0;
        tasks[i].waiting_for_io = 0;
    }
}

void copy_tasks(task_t *dest, task_t *src, int num_tasks)
{
    for (int i = 0; i < num_tasks; i++)
    {
        dest[i] = src[i];
    }
}

void sleep_ms(int milliseconds)
{
    usleep(milliseconds * 1000); // usleep takes sleep time in microseconds
}

double get_time()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

void perform_io(task_t *task)
{
    printf("Task %d performing I/O for %d ms\n", task->id, task->io_time);
    sleep_ms(task->io_time);
    task->io_time = 0;
    task->waiting_for_io = 0;
}