#include <iostream>
#include <sstream>
#include <fstream>
#include <cstdio>
#include <cerrno>
#include <cstdlib>

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "helper_funcs.h"

using namespace std;

int main(int argc ,char **argv) {
    logging << "Doorman #" << getpid() << " has started" << endl;
    int waiting_time = 0;
    key_t key;
	/* Getting arguments */
	if (argc != 5) {
		cerr << "ERROR : Wrong arguments (" << argc << ") for doorman.cpp!" << endl;
		cerr << "EXAMPLE : ./waiter -d period -s shmid" << endl;
		exit(EXIT_FAILURE);
	}
	else {
		int i = 1;
		while (i < argc) {
			if (strncmp(argv[i],"-s",2) == 0)
				key = (key_t) atoi(argv[i + 1]);
			else if (strncmp(argv[i],"-d",2) == 0)
				waiting_time = atoi(argv[i + 1]);
			i = i + 2;
		}
	}
	//logging << "Doorman's argvs shmid : " << key << " waiting_time " << waiting_time << endl;


	time_t t;
	srand((int)time(&t) % getpid());
	Shared_mem_struct* shm;
	int shmid = shmget(key, sizeof(Shared_mem_struct), 0666);
	if (shmid < 0) {
		perror("shmget_doorman");
		exit(EXIT_FAILURE);
	}

	shm = (Shared_mem_struct*) shmat(shmid, NULL, 0);
	if (shm == (Shared_mem_struct*) -1) {
		perror("shmat_doorman");
		exit(EXIT_FAILURE);
	}
	Table *tables = (Table*)((int*)shm + sizeof(Shared_mem_struct));
	int max_cap = Max_cap_of_tables(shm,tables);
	shm->child_synch -= 1;
    while(1) {
    	logging << "\tDoorman::waiting for a task" << endl;
        Sem_wait(shm->doorman,"sem_wait @ doorman ");
    	int pos;
        switch(shm->doorman_task) {
        case 0: // customers were waiting @ queue
        	if (max_cap < shm->cur_group) {
        		shm->doorman_answer = 2;
    			logging << "QUEUE\tDoorman::No table available for such big group("
    					<< shm->cur_group << ")" << endl;
    			break;
        	}
			pos = Search_tables(shm,tables);
			if ((pos >= 0) && (pos < shm->tables_num)
					&& (tables[pos].capacity >= shm->cur_group)) { // found table
				shm->doorman_answer = 0;
				logging << "QUEUE\tDoorman::Table #" << pos << " is available." << endl;
				break;
			}
			if ((pos == shm->tables_num) || (pos == -1)) { // no table available : try & place them @ bar
				logging << "bar cap : " << shm->bar_cap << " - peopleinbar : " << shm->people_in_bar << " cur_group " << shm->cur_group << endl;
				if ((shm->bar_cap - shm->people_in_bar) >= shm->cur_group) {
					shm->doorman_answer = 1;
					shm->people_in_bar += shm->cur_group;
					Bar_calc(shm,shm->cur_group);
					logging << "QUEUE\tDoorman::showing customers they way to the bar" << endl;
					break;
				} else
					logging << "QUEUE\tDoorman::even bar is full!" << endl;
			}
			shm->doorman_answer = 2;
			logging << "QUEUE\tDoorman::Everything is full pos:" << pos << " group : " << shm->cur_group << endl;
			break;
        case 1: // customers were waiting @ bar
			pos = Search_tables(shm,tables);
			if ((pos >= 0) && (pos < shm->tables_num)) { // found table
				shm->doorman_answer = 0;
				logging << "BAR\tDoorman::Table #" << pos << " is available." << endl;
			}
        	break;
        case 2:
			shmdt(shm);
		    logging << "Doorman #" << getpid() << " has ended" << endl;
		    cout << "Doorman #" << getpid() << " has ended" << endl;

		    return 0;
        default:
		    logging << "WRONG_CASE \tDoorman #" << getpid() << " wrong case" << endl;
		    cout << "WRONG_CASE:Doorman #" << getpid() << " wrong case" << endl;

        }
        // inform customer
    	int st = rand()%waiting_time;
    	logging << "ANSWER\tDoorman::Gonna sleep for : " << st << endl;
	    sleep(st);
		logging << "ANSWER\tDoorman::Informing customers about their request" << endl;
        Sem_post(shm->cust_waiting,"doorman post cust_waiting ");
		logging << "WAIT\tDoorman::Waiting customers that just served" << endl;
        Sem_wait(shm->doorman,"sem_wait @ doorman_just served ");
        //if (shm->doorman_answer == 2) sleep(rand()%4); // wait a bit to free some tables
        //1st attempt for bar
         if ((shm->free_tables > 0) && (shm->people_in_bar > 0)) {
        	// awake from bar.
        	int pos = Search_tables(shm,tables);
			if ((pos >= 0) && (pos < shm->tables_num)) { // found table
				shm->doorman_answer = 0;
				shm->cur_table = pos;
				logging << "BAR\tDoorman::Table #" << pos << " is available.Informing bar." << endl;
				switch(tables[pos].capacity) {
				case 8:
					if (shm->b8 == 0) {
						logging << "BAR\tDoorman nobody waiting @ bar8" << endl;
					} else {
						shm->cur_table = pos;
						logging << "BAR\tDoorman::Waking bar8." << endl;
						Sem_post(shm->bar8,"doorman post bar8");
						break;
					}
				case 6:
					if (shm->b6 == 0) {
						logging << "BAR\tDoorman nobody waiting @ bar6" << endl;
					} else {
						shm->cur_table = pos;
						logging << "BAR\tDoorman::Waking bar6." << endl;
						Sem_post(shm->bar6,"doorman post bar6");
						break;
					}
				case 4:
					if (shm->b4 == 0) {
						logging << "BAR\tDoorman nobody waiting @ bar4" << endl;
					} else {
						shm->cur_table = pos;
						logging << "BAR\tDoorman::Waking bar4." << endl;
						Sem_post(shm->bar4,"doorman post bar4");
						break;
					}
				case 2:
					if (shm->b2 == 0) {
						logging << "BAR\tDoorman nobody waiting @ bar2" << endl;
						break;
					} else {
						shm->cur_table = pos;
						logging << "BAR\tDoorman::Waking bar2." << endl;
						Sem_post(shm->bar2,"doorman post bar2");
						break;
					}
				default:
					logging << "WRONG_CASE \tbar." << endl;
				}
			}
        }
    	Sem_post(shm->out_queue,"sem_post doorman @ queue ");
    }
    return 1;
}
