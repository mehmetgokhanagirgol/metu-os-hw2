
#include <iostream>
#include <vector>
#include <sstream>
#include <unistd.h>
#include <sys/time.h>
#include "hw2_output.h"

using namespace std;

struct privateDuties{
    int privateCount;
    int gid;
    int si;
    int sj;
    int tg;
    int ng;
    pthread_t threadid;
    vector<vector<int>> areas;
    pthread_mutex_t mutSignal = PTHREAD_MUTEX_INITIALIZER;
};

struct sergent{
    vector<int> orderTimes;
    vector<string> order;
};

struct box{
    int count;
    pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
};

vector<vector<box>> grid;
sergent captain;
bool halt = false;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond2 = PTHREAD_COND_INITIALIZER;
vector<privateDuties> allDuties;
string order;



static long get_timestamp(struct timeval g_start_time)
{
    struct timeval cur_time;
    gettimeofday(&cur_time, NULL);
    return (cur_time.tv_sec - g_start_time.tv_sec) * 1000000 
           + (cur_time.tv_usec - g_start_time.tv_usec);
}

void unlockAll(){
    for(auto i = 0; i < grid.size(); ++i){
        for(auto j = 0; j < grid[0].size(); ++j){
            pthread_mutex_unlock(&(grid[i][j].mut));
        }
    }
}

void *sergentRun(void *arg){
    timeval *start_time = (timeval *) arg;
    for(auto i = 0; i < captain.order.size(); ++i){
        while(true){
            if(captain.orderTimes[i] < get_timestamp(*start_time) / 1000){
                if(captain.order[i] == "break"){
                    order = "break";
                    pthread_cond_broadcast(&cond);
                    hw2_notify(ORDER_BREAK, 0, 0, 0);
                    break;
                }
                else if(captain.order[i] == "continue"){
                    order = "continue";
                    pthread_cond_broadcast(&cond2);
                    hw2_notify(ORDER_CONTINUE, 0, 0, 0);
                    break;
                }
                else{
                    order = "stop";
                    pthread_cond_broadcast(&cond2);
                    pthread_cond_broadcast(&cond);
                    hw2_notify(ORDER_STOP, 0, 0, 0);
                    break;
                }
            }
        }
    }

    return NULL;
}

void unlockArea(int n, privateDuties *soldier){
    for(auto i = soldier -> areas[n][0]; i < soldier -> areas[n][0] + soldier -> si; ++i){
        for(auto j = soldier -> areas[n][1]; j < soldier -> areas[n][1] + soldier -> sj; ++j){
            pthread_mutex_unlock(&(grid[i][j].mut));
        }
    }
}


void lockArea(int n, privateDuties *soldier){
    vector<vector <int>> lockedAreas;
    int lockrc;
    for(auto i = soldier -> areas[n][0]; i < soldier -> areas[n][0] + soldier -> si; ++i){
        for(auto j = soldier -> areas[n][1]; j < soldier -> areas[n][1] + soldier -> sj; ++j){
            lockrc = pthread_mutex_trylock(&(grid[i][j].mut));
            if(lockrc == 0){
                lockedAreas.push_back({i, j});
            }
            else{
                for(auto k = 0; k < lockedAreas.size(); ++k){
                    pthread_mutex_unlock(&(grid[lockedAreas[k][0]][lockedAreas[k][1]].mut));
                }
                pthread_mutex_lock(&(grid[i][j].mut));
            }
        }
    }
    if(order == "stop"){
        unlockAll();
        hw2_notify(PROPER_PRIVATE_STOPPED, soldier -> gid, 0, 0);
        pthread_exit(NULL);
    }
    else if(order == "break"){
        unlockAll();
        hw2_notify(PROPER_PRIVATE_TOOK_BREAK, soldier -> gid, 0, 0);
        pthread_mutex_lock(&(soldier -> mutSignal));
        pthread_cond_wait(&cond2, &(soldier -> mutSignal));
        pthread_mutex_unlock(&(soldier -> mutSignal));
        if(order == "stop"){
            pthread_exit(NULL);
        }
        hw2_notify(PROPER_PRIVATE_CONTINUED, soldier -> gid, 0, 0);
        lockArea(n, soldier);
    }
}

void gatherFromArea(int n, privateDuties *soldier){
    timeval gatherStart;
    timespec waitTime;
    int rc;
    hw2_notify(PROPER_PRIVATE_ARRIVED, soldier -> gid, soldier -> areas[n][0], soldier -> areas[n][1]);
    for(auto i = soldier -> areas[n][0]; i < soldier -> areas[n][0] + soldier -> si; ++i){
            for(auto j = soldier -> areas[n][1]; j < soldier -> areas[n][1] + soldier -> sj; ++j){
                while(grid[i][j].count > 0){
                    gettimeofday(&gatherStart, NULL);
                    waitTime.tv_nsec = gatherStart.tv_usec * 1000;
                    waitTime.tv_sec = gatherStart.tv_sec;
                    waitTime.tv_sec += (soldier -> tg) / 1000;
                    waitTime.tv_nsec += ((soldier -> tg) % 1000) * 1000000;
                    if(waitTime.tv_nsec > 1000000000){
                        waitTime.tv_sec++;
                        waitTime.tv_nsec -= 1000000000;
                    }
                    pthread_mutex_lock(&(soldier -> mutSignal));
                    rc = pthread_cond_timedwait(&cond, &(soldier -> mutSignal), &waitTime);
                    pthread_mutex_unlock(&(soldier -> mutSignal));
                    if(rc == ETIMEDOUT){
                        grid[i][j].count--;
                        hw2_notify(PROPER_PRIVATE_GATHERED, soldier -> gid, i, j);
                    }
                    else{
                        if(order == "stop"){
                            hw2_notify(PROPER_PRIVATE_STOPPED, soldier -> gid, 0, 0);
                            unlockAll();
                            pthread_exit(NULL);
                        }
                        unlockAll();
                        hw2_notify(PROPER_PRIVATE_TOOK_BREAK, soldier -> gid, 0, 0);
                        pthread_mutex_lock(&(soldier -> mutSignal));
                        pthread_cond_wait(&cond2, &(soldier -> mutSignal));
                        pthread_mutex_unlock(&(soldier -> mutSignal));
                        hw2_notify(PROPER_PRIVATE_CONTINUED, soldier -> gid, 0, 0);
                        lockArea(n, soldier);
                    }

                }
            }
        }
}


void *threadRun(void *arg){
    privateDuties *soldier = (privateDuties *) arg;
    hw2_notify(PROPER_PRIVATE_CREATED, soldier -> gid, 0, 0);
    // For every area
    for(auto n = 0; n < soldier -> ng; ++n){
        // Lock the area
        lockArea(n, soldier);

        // Collect cigbutts - change x-y axises
        gatherFromArea(n, soldier);

        hw2_notify(PROPER_PRIVATE_CLEARED, soldier -> gid, 0, 0);

        // Unlock the area

        unlockArea(n, soldier);

    }
    hw2_notify(PROPER_PRIVATE_EXITED, soldier -> gid, 0, 0);
    return NULL;
}

// gid : private ID
// si, sj : gives out the span of a private
// tg : After collecting a cigbutt private takes a break of tg (before starting to collect the first one it also waits)
// ng : number of areas a private wants to clear

// after these private is given ng amount of top-left coordinates which determines the areas that he want to clear
void takeInput(vector<vector<box>> &grid, vector<privateDuties> &allDuties){

    int width;
    int height;
    int value;
    int counter;
    int temp_count;
    int numberofOrders;
    int tempOrderTime;
    string tempOrder;
    vector<int> tempVector;
    privateDuties tempduty;
    box temp;

    cin >> height;
    cin >> width;

    for(auto i = 0; i < height; ++i){
        grid.push_back({});
        for(auto j = 0; j < width; ++j){
            cin >> temp_count;
            temp.count = temp_count;
            grid[i].push_back(temp);
        }
    }

    cin >> counter;

    for(auto i = 0; i < counter; ++i){
        allDuties.push_back(tempduty);
        allDuties[i].privateCount = counter;
        cin >> allDuties[i].gid;
        cin >> allDuties[i].si;
        cin >> allDuties[i].sj;
        cin >> allDuties[i].tg;
        cin >> allDuties[i].ng;
        for(auto j = 0; j < allDuties[i].ng; ++j){
            allDuties[i].areas.push_back({});
            cin >> temp_count;
            allDuties[i].areas[j].push_back(temp_count);
            cin >> temp_count;
            allDuties[i].areas[j].push_back(temp_count);
        }
    }

    if(!(cin >> numberofOrders))
        return;

    for(auto i = 0; i < numberofOrders; ++i){
        cin >> tempOrderTime;
        captain.orderTimes.push_back(tempOrderTime);
        cin >> tempOrder;
        captain.order.push_back(tempOrder);
    }

}

int main(){
    timeval g_start_time;
    gettimeofday(&g_start_time, NULL);
    hw2_init_notifier();
    // Take the input
    int count;

    

    takeInput(grid, allDuties);
    pthread_t thread_arr[(allDuties[0].privateCount)+1];
    count = allDuties[0].privateCount;
    pthread_create(&thread_arr[0], NULL, sergentRun, &g_start_time);

    for(auto i = 0; i < count; ++i){
        pthread_create(&thread_arr[i+1] , NULL, threadRun, &allDuties[i]);
    }
    for(auto i= 0; i < count+1; ++i){
        pthread_join(thread_arr[i], NULL);
    }

    return 0;
}