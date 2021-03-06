#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "pthread_sleep.c"
#include "utils.c"


char* getCurrentTime();
void updateLogCarFile(Car *car);
void updateLogPoliceFile(char *message);
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t laneConditions[4];

pthread_cond_t iteration_finish_condition;
pthread_cond_t police_work_condition;
pthread_cond_t horn_condition;
pthread_t lane_threads[4];

Queue *queues[4]; // {N, E, S, W}

char currentLane = 0;

FILE *carLog;
FILE *policeLog;
char currentTimeString[10];
int ID = 0;
int cell_phone_delay = 0;
int relative_time = 0;
int north_timer = 0; //Used to check 20 second rule for north lane.
int north_sleep_flag=0;
int checkMoreThanFiveCar(){
    for(int i=0; i<4; i++){
        if(queues[i]->carCount >= 5){
            return 1;
        }
    }
    return 0;
}

int checkIfAllLanesEmpty(){
    for(int i=0; i<4; i++){
        if(queues[i]->carCount > 0){
            return 0;
        }
    }
    return 1;
}

int getTheMostCrowdedLane(){
    int currentCount = 0;
    int lane;
    for(int i=3; i>=0; i--){
        if(queues[i]->carCount >= currentCount){
            lane = i;
            currentCount = queues[i]->carCount;
        }
    }
    return lane;
}

void printLanes(){
    printf("\t%d\n", queues[0]->carCount);
    printf("%d\t\t%d\n", queues[3]->carCount, queues[1]->carCount);
    printf("\t%d\n", queues[2]->carCount);
}

void *lane(void *direction){
    int i = (int) direction;
    pthread_mutex_lock(&lock);

    while(1){
        pthread_cond_wait(&laneConditions[i], &lock);

        if(cell_phone_delay !=0){
            pthread_cond_signal(&horn_condition);
            updateLogPoliceFile("Honk");
            pthread_sleep(1);
        }else{
            Car *car = dequeue(queues[i]);
            pthread_sleep(1);
            strcpy(car->cross_time, getCurrentTime());
            updateLogCarFile(car);
        }

        // updateLogCarFile(car);
        pthread_cond_signal(&iteration_finish_condition);
        // pthread_mutex_unlock(&lock);
    }
}
int getQueueWaitTime(Car *car) {
    int waitTime = 0;
    char delim[] = ":";
    char t[9];
    strcpy(t, car->arrival_time);
    int arrival_h = atoi(strtok(t, delim));
    int arrival_m = atoi(strtok(NULL, delim));
    int arrival_s = atoi(strtok(NULL, delim));

    char time [9];
    strcpy(time, getCurrentTime());
    int wait_h = atoi(strtok(time, delim));
    int wait_m = atoi(strtok(NULL, delim));
    int wait_s = atoi(strtok(NULL, delim));

    int h = wait_h - arrival_h;
    int m = wait_m - arrival_m;
    int s = wait_s - arrival_s;

    waitTime = h * 3600 + m * 60 + s;

    return waitTime;
}
int checkCarsWaitTime() {

    for(int i=0; i<4; i++){

            for(int j = queues[i]->front; j<= queues[i]->rear; j++){
                if (queues[i]->front == -1 || queues[i]->rear == -1) continue;
                if (queues[i]->cars[j] != NULL){
                    int d = getQueueWaitTime(queues[i]->cars[j]);
                    if (d >= 20) {
                        return i;
                    }
                }

            }


    }
    return -1;
}

void *police_officer_function(){
    pthread_mutex_lock(&lock);

    while(1){

    pthread_cond_wait(&police_work_condition, &lock);
    if(checkIfAllLanesEmpty()){
        cell_phone_delay = 3;
        pthread_sleep(1);
        updateLogPoliceFile("Cell Phone");
        pthread_cond_signal(&iteration_finish_condition);
    }

    if (cell_phone_delay != 0){
        currentLane = getTheMostCrowdedLane();
        pthread_cond_signal(&laneConditions[currentLane]);
        pthread_cond_wait(&horn_condition, &lock);
        cell_phone_delay--;
    }else {
        int delayedLane = checkCarsWaitTime();
        if (delayedLane != -1) {
            currentLane = delayedLane;
            pthread_cond_signal(&laneConditions[currentLane]);
        }
        else if(checkMoreThanFiveCar()){
            currentLane = getTheMostCrowdedLane();
            pthread_cond_signal(&laneConditions[currentLane]);

        }else if(queues[currentLane]->carCount == 0){
            //N>E>S>W
            for(int i=0; i<4; i++){
                if(queues[i]->carCount != 0){
                    currentLane=i;
                    pthread_cond_signal(&laneConditions[currentLane]);
                    break;
                }
            }
        }else{
            //don't change current line
            pthread_cond_signal(&laneConditions[currentLane]);
        }
    }
    }
}

Car *createCar(char direction) {
    Car *car = (struct Car*) malloc(sizeof(Car));

    ID++;
    car->id = ID;
    strcpy(car->arrival_time, getCurrentTime());

    switch (direction){
    case 'N':
        car->direction = 'N';
        break;
    case 'E':
        car->direction = 'E';
        break;
    case 'S':
        car->direction = 'S';
        break;
    case 'W':
        car->direction = 'W';
        break;
    default:
        break;
    }
    return car;
}

void addCar(double p) {

    //implement car struct -
    //implement queue struct -
    //create cars with random probability  

    double Nprob = rand() % 100;
    double Sprob = rand() % 100;
    double Wprob = rand() % 100;
    double Eprob = rand() % 100;

    p = p*100;

    Car *car;

    if(!north_sleep_flag){
        if (Nprob >= p){
        enqueue(queues[0], createCar('N'));
        }else{
            north_sleep_flag = 1;
            north_timer = relative_time + 20;
        }
    }else {
        if (north_timer == relative_time){
            north_sleep_flag = 0;
            enqueue(queues[0], createCar('N'));
        }
    }


    if (Eprob < p)
    {
        enqueue(queues[1], createCar('E'));
    }
    if (Sprob < p)
    {
        enqueue(queues[2], createCar('S'));
    }
    if (Wprob < p)
    {
        enqueue(queues[3], createCar('W'));
    }

}

void initializeLaneQueues() {
    //initialize queue lanes
    for(int i = 0; i < 4; i++){
        queues[i] = (struct Queue*) malloc(sizeof(Queue));
        queues[i]->front = -1;
        queues[i]->rear = -1;
        // {N, E, S, W}
        switch (i) {
            case 0:
                queues[i]->direction = 'N';
                break;
            case 1:
                queues[i]->direction = 'E';
                break;
            case 2:
                queues[i]->direction = 'S';
                break;
            case 3:
                queues[i]->direction = 'W';
                break;
        }
    }



    //at t=0 all lanes have a car
    enqueue(queues[0], createCar('N'));

    enqueue(queues[1], createCar('E'));
    
    enqueue(queues[2], createCar('S'));

    enqueue(queues[3], createCar('W'));

}

int getWaitTime(Car *car){
    int waitTime = 0;
    char delim[] = ":";
    char t[9];
    strcpy(t, car->arrival_time);
    int arrival_h = atoi(strtok(t, delim));
    int arrival_m = atoi(strtok(NULL, delim));
    int arrival_s = atoi(strtok(NULL, delim));

    char c[9];
    strcpy(c, car->cross_time);
    int cross_h = atoi(strtok(c, delim));
    int cross_m = atoi(strtok(NULL, delim));
    int cross_s = atoi(strtok(NULL, delim));

    int h = cross_h - arrival_h;
    int m = cross_m - arrival_m;
    int s = cross_s - arrival_s;

    waitTime = h * 3600 + m * 60 + s;

    return waitTime;
}

void updateLogCarFile(Car *car){
    //Add car to log file //CarID Direction Arrival-Time Cross-Time Wait-Time
    char logMsg[100];
    sprintf(logMsg, "%d  \t\t%c  \t\t%s  \t\t%s  \t\t%d \n", car->id, car->direction, car->arrival_time, car->cross_time, getWaitTime(car));
    fprintf(carLog, "%s", logMsg);
}

void updateLogPoliceFile(char *message){
    char logMsg[100];
    sprintf(logMsg, "%s\t%s\n", getCurrentTime(), message);
    fprintf(policeLog, "%s", logMsg);
}

char* getCurrentTime(){
    time_t rawtime;
    struct tm * timeinfo;

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    sprintf(currentTimeString, "%d:%d:%d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    return currentTimeString;
}

void initializeLogFiles(){
    //initialize log file

    carLog = fopen("car.log", "w");
    policeLog = fopen("police.log", "w");
    if(carLog == NULL && policeLog == NULL)
    {
        perror("Program crashed.\n");
        exit(1);
    }
    fprintf(carLog,"CarID\tDirection\tArrival-Time\tCross-Time\tWait-Time \n");
    fprintf(carLog,"----------------------------------------------------------------------------------------------------------------\n");

    fprintf(policeLog,"Time\tEvent\n");
    fprintf(policeLog,"-----------------------------------------------------------------------------------------------\n");
}


int main(int argc, char const *argv[]){
    pthread_t lane_queues[4];
    // Args: -s timeLog(arg1) simulationTime(arg2) probability(arg3) seed(arg4) snapshot(arg5)

    //simulationTime

    int simulationTime = 0;
    simulationTime = atoi(argv[2]);

        //get probability
        double prob = 0.0;
        prob = atof(argv[3]);

        //get seed
        int seed = 0;
        seed = atoi(argv[4]);

        //get t for snapshot
        int snapshot_time = atoi(argv[5]);

        //set seed
        srand(seed);

    initializeLogFiles();
    initializeLaneQueues();

    pthread_t police_officer_thread;

    /* Initialize mutex and condition variable objects */
    pthread_mutex_init(&lock, NULL);

    for(int i=0; i<4; i++){
        pthread_cond_init(&laneConditions[i], NULL);
    }

    pthread_cond_init(&police_work_condition, NULL);
    pthread_cond_init(&iteration_finish_condition, NULL);
    pthread_cond_init(&horn_condition, NULL);



    for(int i=0; i<4; i++){
        pthread_create(&lane_threads[0], NULL, lane, (void *) i);
    }

    pthread_create(&police_officer_thread, NULL, police_officer_function, NULL);

    sleep(2); //give chance processes to get initialized.

    while(relative_time <= simulationTime){
        pthread_mutex_lock(&lock);
        pthread_cond_signal(&police_work_condition);

        pthread_cond_wait(&iteration_finish_condition, &lock);
        addCar(prob);
        relative_time++;

        if(snapshot_time < relative_time){
            printLanes();
        }

        pthread_mutex_unlock(&lock);
}

    return 0;
}



