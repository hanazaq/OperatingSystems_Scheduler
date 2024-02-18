#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Process */
typedef struct
{
    char Done;           // 0 no, 1 yes
    char name[20];       // e.g. P1
    int priority;        // higher priorirty >> more important process
    int mapped_priority; // index of its queue in roundrobin {0,..,9}
    int type;            //{"PLATINUM":3, "GOLD":2, "SILVER":1}
    int arrival_time;
    int total_burst_time;   // total sum of inst_time
    int completion_time;    // to be calculated
    int last_executed_line; // initially 0
    int inst_time[20];      // no P.txt file has more than 20 lines
    int inst_count;         // length of inst_time array
    int enter_exec;         // how many times the process enters the ready queue (important to switch type G,S,P)
    int new_arrival_time;   // used for GOLD and SILVER processes when they renter the round robin
} Process;
/*(This is the most imporant array in the project, it stored all process structs in the order they came in input,
and along the whole program processes will be accessed from here using their index which is fixed all the way long)*/
Process processes[10]; // at most 10 processes
int total_num_p = 0;   // length of processes array

/* Arrival Time */
int arrival_times[10];       // repeations allowed , length = total_num_p
int final_arrival_times[10]; // remove repeations
int stations = 0;            // length of final_arrival_times
int arrived_p[10][11];       // max
/*
All above variables used to build this structure:
final_arrival_times      0 |  10   | 170
arrived_p[][0]           2 |   3   |  2
index_of_p[][1_11]       2 |  0,1  |  3
Access processes        e.g. processes[2]
*/
int const MAX_LINE_SIZE = 250;
int const QUEUE_CAPACITY = 10;
int instructions_burst_time[21]; // fixed
int priorirties[10];             // repeations allowed, has priorities of all processes stored in ascending order
int final_priorirties[10];       // with no repeation
int len_final_priorirties = 0;   // length of final_priorities
/* Scheduling variables*/
int PLATINUM_readyQ[10];           // ready queue keep track of platinum processes' indexes which had arrived; values here are stored based on priority> arrival time> alphapet
int len_PLATINUM_readyQ = 0;       // length of PLATINUM_readyQ array;
int done_p = 0;                    // total number of processes done execution in the program
int next_station = 0;              // station represent arrival time (e.g. at 80 ms)
int current_highest_priority = -1; // holds the index of the prioirty queue in roundrobin
int last_process_executed = -1;    // used to prevent context switches when same process executed twice

//////////////////////////////////////////////////////////////////////////////////////////////////////
/* implemented methods */
int compare(const void *, const void *); // compare number to sort in ascending order (used for arrival times)
int compareStructs(Process, Process);    // priority > arrival_time> name
void insertionSort(int[], int, int);     // insert to the list based on compareStructs (for PLATINUM_readyQ list)
int load_at_station(int);                // insert processes at given arrival station to PLATINUM_readyQ, update len_PLATINUM_readyQ
int load_till_endt(int);                 // insert processes with arrival time <=endt and station>= next station to PLATINUM_readyQ, update len_PLATINUM_readyQ
int process_one_PLATINUM(int);           // does the changes needed to a platinum process after finishes its burst time in cpu
int process_one_GS(int);                 // does the changes needed to a GS process after finishes its quantum time in cpu
int schedule();                          // manage the execution of all processes
void calculate();                        // calculate final avg waiting time and avg turn around time
int read_input();                        // reading data from files
void printing();                         // print structs values
int check_priorities();                  // remove repeated priorities and fill final_priorirties. Also, assign each process its mapped_priority
int check_p_arrival();                   // remove repeated arrival times
void distribute_to_queues(int[], int);   // given indexes of processes, distribute them among round robin queues acoording to their priorities
void edit(int);                          // used when GS process renter the roundrobin queue to find its suitable index in the queue. (comparing arrival times and names with other processes in the queue)
struct Queue *createQueue();             // create queue structs
int isFull(struct Queue *);              // check if queue is full
int isEmpty(struct Queue *);             // check if queue is empty
void enqueue(struct Queue *, int);       // enqueue to the queue given in param
int dequeue(struct Queue *);             // dequeue from queueu given in param

//////////////////////////////////////////////////////////////////////////////////////////////////////
/* QUEUE Implementation*/
struct Queue
{
    int front;
    int rear;
    int size;
    int *array;
};
struct Queue *round_robin[10]; // for each priority queue

// initializes size of queue as 0
struct Queue *createQueue()
{
    struct Queue *queue = (struct Queue *)malloc(sizeof(struct Queue));
    queue->front = 0;
    queue->rear = QUEUE_CAPACITY - 1; // helpful in enqueue
    queue->size = 0;
    queue->array = (int *)malloc(QUEUE_CAPACITY * sizeof(int));
    return queue;
}

// Queue is full when size becomes equal to the capacity
// the queue not will overflow since maximum there will be 10 gold/silver process in the while input
int isFull(struct Queue *queue)
{
    return (queue->size == QUEUE_CAPACITY);
}

// Queue is empty when size is 0
int isEmpty(struct Queue *queue)
{
    return (queue->size == 0);
}

////////////////////////////////////////////////////////////////////////////////////////
/* compartors */

int compare(const void *a, const void *b)
{
    return (*(int *)a - *(int *)b);
}

// compare two Processes according to priority > arrival_time> name
int compareStructs(Process a, Process b)
{
    if (a.priority != b.priority) // larger number higher priority
    {
        return b.priority - a.priority;
    }
    if (a.arrival_time != b.arrival_time) // earlier arrival time higher priority
    {
        return a.arrival_time - b.arrival_time;
    }
    return strcmp(a.name, b.name); // sort alphapetically
}

// Insertion new_process based on compareStructs
void insertionSort(int arr[], int old_size, int new_process)
{
    int i = old_size - 1;
    // Shift processes with lower priority to right
    while (i >= 0 && compareStructs(processes[arr[i]], processes[new_process]) > 0)
    {
        arr[i + 1] = arr[i];
        i--;
    }
    // Insert the new process_index in the correct position
    arr[i + 1] = new_process;
}

////////////////////////////////////////////////////////////////////////////////////////
/* Solving Technical issues */

int check_priorities()
{
    // give every process its mapping to priority used in accessing queues later
    qsort(priorirties, total_num_p, sizeof(int), compare);
    /* remove repeated priorities e.g. 0 1 1 3 to 0 1 3 */
    for (int p = 0; p < total_num_p; p++)
    {
        if (p == total_num_p - 1 || priorirties[p] != priorirties[p + 1])
        {
            final_priorirties[len_final_priorirties] = priorirties[p];
            len_final_priorirties++;
        }
    }
    // final the information in each process
    for (int i = 0; i < total_num_p; i++)
    {
        for (int j = 0; j < len_final_priorirties; j++)
        {
            if (processes[i].priority == final_priorirties[j])
            {
                processes[i].mapped_priority = j;
                break;
            }
        }
    }
}

int check_p_arrival()
{
    qsort(arrival_times, total_num_p, sizeof(int), compare);
    /* remove repeated arrival times e.g. 0 1 1 3 to 0 1 3 */
    for (int p = 0; p < total_num_p; p++)
    {
        if (p == total_num_p - 1 || arrival_times[p] != arrival_times[p + 1])
        {
            final_arrival_times[stations] = arrival_times[p];
            stations++;
        }
    }
    for (int s = 0; s < stations; s++)
    {
        int a = 1;
        for (int p = 0; p < total_num_p; p++)
        {
            if (processes[p].arrival_time == final_arrival_times[s])
            {
                arrived_p[s][a] = p; // has index of that process you can reach it
                a++;
            }
        }
        arrived_p[s][0] = a; // length of the list
    }

    return 0;
}

void edit(int queue_index)
{
    if (isEmpty(round_robin[queue_index]) || round_robin[queue_index]->size == 1)
        return;

    int rear = round_robin[queue_index]->rear;
    int last_element = round_robin[queue_index]->array[rear];
    if (processes[last_element].new_arrival_time == -1)
    {
        return;
    }
    int position = rear - 1;
    while (position >= round_robin[queue_index]->front &&
           (processes[round_robin[queue_index]->array[position]].arrival_time >= processes[last_element].new_arrival_time ||
            (processes[round_robin[queue_index]->array[position]].arrival_time == processes[last_element].new_arrival_time &&
             strcmp(processes[round_robin[queue_index]->array[position]].name, processes[last_element].name) > 0)))
    {

        // Swap elements
        int temp = round_robin[queue_index]->array[position];
        round_robin[queue_index]->array[position] = round_robin[queue_index]->array[position + 1];
        round_robin[queue_index]->array[position + 1] = temp;

        position--;
    }
}

void distribute_to_queues(int arr[], int len)
{
    // add to the queue which has same priority of each
    for (int i = 0; i < len; i++)
    {
        enqueue(round_robin[processes[arr[i]].mapped_priority], arr[i]);
    }
}

// NOTE: we initialized queue->rare with capacity-1, so on the first enqueue to the queue will be (9+1)%10=0 Awesome!!
void enqueue(struct Queue *queue, int item)
{
    if (isFull(queue))
        return;
    // add the new element to the end of the queue
    queue->rear = (queue->rear + 1) % QUEUE_CAPACITY;
    queue->array[queue->rear] = item;
    queue->size = queue->size + 1;
}

// it changes front and size
int dequeue(struct Queue *queue)
{
    if (isEmpty(queue))
        return -1;
    // dequeue element from the beginning of the queue
    int item = queue->array[queue->front];
    // change the pointer that points to the beginning of the queue
    queue->front = (queue->front + 1) % QUEUE_CAPACITY;
    queue->size = queue->size - 1;
    return item;
}
////////////////////////////////////////////////////////////////////////////////////////
/* logic of the scheduling */

// load processes at given arrival time
int load_at_station(int this_station)
{
    int GS_readyQ[10];     // ready queue keep track of gold and silver processes' indexes which had arrived; values here are stored based on priority> arrival time> alphapet
    int len_GS_readyQ = 0; // length of GS_readyQ array;
    for (int w = 1; w < arrived_p[this_station][0]; w++)
    {
        if (processes[arrived_p[this_station][w]].type == 3) // PLATINUM
        {
            insertionSort(PLATINUM_readyQ, len_PLATINUM_readyQ, arrived_p[this_station][w]);
            len_PLATINUM_readyQ++;
        }
        else // gold or silver
        {
            if (processes[arrived_p[this_station][w]].mapped_priority > current_highest_priority)
            {
                current_highest_priority = processes[arrived_p[this_station][w]].mapped_priority;
            }
            insertionSort(GS_readyQ, len_GS_readyQ, arrived_p[this_station][w]);
            len_GS_readyQ++;
        }
    }
    distribute_to_queues(GS_readyQ, len_GS_readyQ);
}

// loaded processes that are not loaded and had arrived before end_t
int load_till_endt(int end_t)
{
    int GS_readyQ[10];     // ready queue keep track of gold and silver processes' indexes which had arrived; values here are stored based on priority> arrival time> alphapet
    int len_GS_readyQ = 0; // length of GS_readyQ array;
    next_station++;
    int s = next_station;
    // traverse the arrival times that happened before or during end_t
    while (final_arrival_times[s] <= end_t && s < stations)
    {
        for (int w = 1; w < arrived_p[s][0]; w++)
        {
            if (processes[arrived_p[s][w]].Done == 0)
            {
                if (processes[arrived_p[s][w]].type == 3) // PLATINUM
                {
                    insertionSort(PLATINUM_readyQ, len_PLATINUM_readyQ, arrived_p[s][w]);
                    len_PLATINUM_readyQ++;
                }
                else // gold or silver
                {
                    if (processes[arrived_p[s][w]].mapped_priority > current_highest_priority)
                    {
                        current_highest_priority = processes[arrived_p[s][w]].mapped_priority;
                    }
                    insertionSort(GS_readyQ, len_GS_readyQ, arrived_p[s][w]);
                    len_GS_readyQ++;
                }
            }
        }

        s++;
    }
    // update next_station
    next_station = s - 1; // check this later
    distribute_to_queues(GS_readyQ, len_GS_readyQ);
}

// updates on process data after finishing the burst time
int process_one_PLATINUM(int time_now)
{
    Process *current = &processes[PLATINUM_readyQ[0]];
    last_process_executed = PLATINUM_readyQ[0];
    current->completion_time = time_now + 10 + current->total_burst_time;
    current->last_executed_line = current->inst_count; // not important
    current->Done = 1;
    done_p++;
    // delete the 0th index from the array
    int shift_i = 0;
    while (shift_i < len_PLATINUM_readyQ - 1)
    {
        PLATINUM_readyQ[shift_i] = PLATINUM_readyQ[shift_i + 1];
        shift_i++;
    }
    len_PLATINUM_readyQ--;
    int end_time = current->completion_time;
    load_till_endt(end_time);
    return current->completion_time;
}

int process_one_GS(int time_now)
{
    int index = dequeue(round_robin[current_highest_priority]);
    Process *current = &processes[index];

    int quantum = 120;      // GOLD
    if (current->type == 1) // SILVER
    {
        quantum = 80;
    }
    int old_highest_priority = current_highest_priority;
    int time_spent_inCPU = 0;
    char flag_enterance_cs = 0; // add the time of context switch only one time before the first load opetation
    int executed_quantum = 0;
    /*
    Logic: execute atomic > load till end time > quite if:
    1. platunim comes
    2. highest priority changes (gold silver with higher priority came)
    3. time quantum finished and current queue size > 1
    */
    // execute atomic instruction
    while (current->last_executed_line < current->inst_count)
    {
        // execute line by line
        time_spent_inCPU += current->inst_time[current->last_executed_line];
        executed_quantum += current->inst_time[current->last_executed_line];
        current->last_executed_line++;
        // load recently arrived processes
        int end_time = time_now + time_spent_inCPU;
        if (flag_enterance_cs == 0 && last_process_executed != index) // true only once in this function
        {
            flag_enterance_cs = 1;
            time_now += 10; // context switch
        }
        if (executed_quantum >= quantum)
        {
            executed_quantum = 0;
            current->enter_exec++; // how many times was this process executed?
        }
        load_till_endt(end_time); // new processes has been added

        if (len_PLATINUM_readyQ > 0)
        {
            break;
        }
        if (old_highest_priority != current_highest_priority)
        {
            break;
        }
        if (time_spent_inCPU >= quantum && round_robin[current_highest_priority]->size > 0)
        {
            break;
        }
    }
    last_process_executed = index;
    /* upgrade gold to platinum for 5 quantum times*/
    if (current->type == 2 && current->enter_exec == 5 && current->last_executed_line < current->inst_count)
    {
        // becomes PLATINUM
        current->type = 3;
        current->enter_exec = 0;
        // lets excute the platunim right now if PLATINUM_readyQ is empty or the new paltinum has the highest priority.
        if (len_PLATINUM_readyQ == 0 || current->priority > processes[PLATINUM_readyQ[0]].priority)
        {
            // execute all left instructions
            while (current->last_executed_line < current->inst_count)
            {
                time_spent_inCPU += current->inst_time[current->last_executed_line];
                current->last_executed_line++;
            }
            load_till_endt(time_now + time_spent_inCPU);
        }
        // it is not the highest priority platunim, add to platunim_todo array
        else
        {
            insertionSort(PLATINUM_readyQ, len_PLATINUM_readyQ, index);
            len_PLATINUM_readyQ++;
        }
    }
    // upgrade silver to gold
    else if (current->type == 1 && current->enter_exec == 3 && current->last_executed_line < current->inst_count)
    {
        // becomes GOLD
        current->type = 2;
        current->enter_exec = 0;
    }
    current->completion_time = time_now + time_spent_inCPU;

    // if all instructions are done
    if (current->last_executed_line == current->inst_count)
    {
        current->Done = 1;
        done_p++;
        // find new highest priority
        if (current_highest_priority == current->mapped_priority && round_robin[current_highest_priority]->size == 0)
        {
            char found = 0;
            for (int i = current_highest_priority - 1; i >= 0; i--)
            {
                if (round_robin[i]->size > 0)
                {
                    found = 1;
                    current_highest_priority = i;
                    break;
                }
            }
            // no gold and silver left
            if (found == 0)
            {
                current_highest_priority = -1;
            }
        }
    }
    else
    {
        current->new_arrival_time = current->completion_time;
        enqueue(round_robin[current->mapped_priority], index);
        edit(current->mapped_priority);
    }

    return current->completion_time;
}

int schedule()
{
    int time_now = 0;   // starting running
    load_at_station(0); // there is always a process coming at t=0
    while (done_p < total_num_p)
    {
        if (current_highest_priority == -1 && len_PLATINUM_readyQ == 0)
        {
            load_at_station(++next_station);              // load processes there
            time_now = final_arrival_times[next_station]; // jump to next arrival point
        }
        else if (len_PLATINUM_readyQ >= 1)
        {
            int value = time_now;
            time_now = process_one_PLATINUM(value);
        }
        else if (current_highest_priority >= 0)
        {
            int value = time_now;
            time_now = process_one_GS(value);
        }
    }
    return 0;
}

void calculate()
{
    int total_waiting_time = 0;
    int total_turn_around_time = 0;

    for (int i = 0; i < total_num_p; i++)
    {
        int p_turnaround_time = processes[i].completion_time - processes[i].arrival_time;
        total_turn_around_time += p_turnaround_time;
        total_waiting_time += (p_turnaround_time - processes[i].total_burst_time);
    }
    double avg_w = (double)total_waiting_time / total_num_p;
    double avg_turn = (double)total_turn_around_time / total_num_p;
    if (avg_w == (int)avg_w)
    {
        printf("%d", (int)avg_w);
    }
    else
    {
        printf("%.1f", avg_w);
    }
    printf("\n");
    if (avg_turn == (int)avg_turn)
    {
        printf("%d", (int)avg_turn);
    }
    else
    {
        printf("%.1f", avg_turn);
    }
    printf("\n");
}
////////////////////////////////////////////////////////////////////////////////////////

/* Setup the initial values*/
int read_input()
{
    /* definition.txt */
    FILE *file = fopen("definition.txt", "r");
    if (file == NULL)
    {
        perror("Error opening file");
        return 1;
    }
    char line[MAX_LINE_SIZE];
    int i = 0;
    while (fgets(line, sizeof(line), file) != NULL)
    {
        char *token = strtok(line, " ");
        strcpy(processes[i].name, token);
        token = strtok(NULL, " ");
        processes[i].priority = atoi(token);
        priorirties[i] = atoi(token);
        token = strtok(NULL, " ");
        processes[i].arrival_time = atoi(token);
        arrival_times[i] = atoi(token);
        token = strtok(NULL, " ");
        if (token[strlen(token) - 1] == '\n')
        {
            token[strlen(token) - 1] = '\0';
        }
        if (strcmp(token, "PLATINUM") == 0)
        {
            processes[i].type = 3;
        }
        else if (strcmp(token, "GOLD") == 0)
        {
            processes[i].type = 2;
        }
        else if (strcmp(token, "SILVER") == 0)
        {
            processes[i].type = 1;
        }
        i++;
    }
    total_num_p = i;
    fclose(file);

    /* instructions.txt */
    FILE *inst_file = fopen("instructions.txt", "r");
    if (inst_file == NULL)
    {
        perror("Error opening inst_file");
        return 1;
    }
    char line2[MAX_LINE_SIZE];
    int j = 0;
    while (fgets(line2, sizeof(line2), inst_file) != NULL)
    {
        char *token = strtok(line2, " ");
        token = strtok(NULL, " ");
        if (token[strlen(token) - 1] == '\n')
        {
            token[strlen(token) - 1] = '\0';
        }
        instructions_burst_time[j] = atoi(token);
        j++;
    }
    fclose(inst_file);

    /* P.txt  */
    for (int p = 0; p < total_num_p; p++)
    {
        char process_name[100];
        strcpy(process_name, processes[p].name);
        strcat(process_name, ".txt");
        FILE *p_file = fopen(process_name, "r");
        if (p_file == NULL)
        {
            perror("Error opening p_file");
            return 1;
        }
        int k = 0;
        // initializaton
        processes[p].last_executed_line = 0;
        processes[p].total_burst_time = 0;
        processes[p].Done = 0;
        processes[p].enter_exec = 0;
        processes[p].new_arrival_time = -1;
        while (fgets(line, sizeof(line), p_file) != NULL)
        {
            char *token = strtok(line, "instr");
            int inst_time = 0;
            if (strcmp(token, "ex") == 0)
            {
                inst_time = instructions_burst_time[20];
            }
            else
            {
                inst_time = instructions_burst_time[atoi(token) - 1];
            }
            processes[p].inst_time[k] = inst_time;
            processes[p].total_burst_time += inst_time;
            k++;
        }
        fclose(p_file);
        processes[p].inst_count = k;
    }
}

void printing()
{
    for (int p = 0; p < total_num_p; p++)
    {
        printf("Name: %s\n", processes[p].name);
        printf("Done: %d\n", processes[p].Done);
        printf("Priority: %d\n", processes[p].priority);
        printf("Mapped Priority: %d\n", processes[p].mapped_priority);
        printf("Type: %d\n", processes[p].type);
        printf("Arrival Time: %d\n", processes[p].arrival_time);
        printf("Total Burst Time: %d\n", processes[p].total_burst_time);
        printf("Completion Time: %d\n", processes[p].completion_time);
        printf("Last Executed Line: %d\n", processes[p].last_executed_line);
        printf("Instruction count: %d\n", processes[p].inst_count);
        printf("Enter Execution: %d\n", processes[p].enter_exec);
        printf("Instruction Times: ");
        for (int i = 0; i < processes[p].inst_count; i++)
        {
            printf("%d ", processes[p].inst_time[i]);
        }
        printf("\n*****************\n");
    }
    printf("Arrival times: \n");
    for (int i = 0; i < total_num_p; i++)
        printf("%d ", arrival_times[i]);
    printf("\nstations %d \n", stations);
    for (int i = 0; i < stations; i++)
    {
        printf("%d ", final_arrival_times[i]);
    }
    printf("\n*****************\n");
    for (int x = 0; x < stations; x++)
    {
        printf("a: %d \nindexes:", arrived_p[x][0]);
        for (int y = 1; y < arrived_p[x][0]; y++)
        {
            printf("%d ", arrived_p[x][y]);
        }
        printf("\n");
    }
    printf("\n*****************\n");
}

////////////////////////////////////////////////////////////////////////////////////////
/* MAIN */
int main()
{
    read_input();
    // preparing data structures for the upcoming stage
    check_p_arrival();
    check_priorities();
    // printing();
    // each priority has its round robin queue
    for (int i = 0; i < len_final_priorirties; i++)
    {
        round_robin[i] = createQueue();
    }
    // so important! organizing the flow of the program
    schedule();
    // print final results
    calculate();
    return 0;
}
