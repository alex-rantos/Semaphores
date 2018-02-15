#include <iostream>
#include <sstream>
#include <fstream>
#include <cstdio>
#include <cerrno>
#include <cstdlib>

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "helper_funcs.h"

using namespace std;

int main(int argc ,char **argv) {
    logging << "Waiter #" << getpid() << " has started" << endl;
    int money_amount = 0, waiting_time = 0;
    key_t key;
	/* Getting arguments */
	if (argc != 7) {
		cerr << "ERROR : Wrong arguments (" << argc << ") for waiter.cpp!" << endl;
		cerr << "EXAMPLE : ./waiter -d period -m moneyamount -s shmid" << endl;
		exit(EXIT_FAILURE);
	}
	else {
		int i = 1;
		while (i < argc) {
			if (strncmp(argv[i],"-m",2) == 0)
				money_amount = atoi(argv[i + 1]);
			else if (strncmp(argv[i],"-s",2) == 0)
				key = (key_t) atoi(argv[i + 1]);
			else if (strncmp(argv[i],"-d",2) == 0)
				waiting_time = atoi(argv[i + 1]);
			i = i + 2;
		}
	}
	//logging << "Waiter's argvs : bill : " << money_amount << " shmid : " << key << " waiting_time " << waiting_time << endl;
	time_t t;
	srand((int)time(&t) % getpid());
	Shared_mem_struct* shm;
	int shmid = shmget(key, sizeof(Shared_mem_struct), 0666);
	if (shmid < 0) {
		perror("shmget_waiter");
		exit(EXIT_FAILURE);
	}
	shm = (Shared_mem_struct*) shmat(shmid, NULL, 0);
	if (shm == (Shared_mem_struct*) -1) {
		perror("shmat_waiter");
		exit(EXIT_FAILURE);
	}
	Table *tables = (Table*)((int*)shm + sizeof(Shared_mem_struct));
	shm->child_synch -= 1;
	while(1) {
		logging << "\tWaiter #" << getpid() << " waiting for customers." << endl;
		Sem_wait(shm->waiters,"wait waiter");
		int table_num = shm->waiter_table;
		int st = rand()%waiting_time + 1;
		switch(shm->waiter_task) {
			case 0: // get order
				tables[table_num].waiter = getpid();
				logging << "ORDER\tWaiter #" << getpid() << " gonna sleep for : " << st << endl;
			    sleep(st);
				logging << "ORDER\tWaiter #" << getpid() << " is getting an order from customers #" <<
					tables[table_num].customers	<< endl;
				tables[table_num].waiter_answer = 0;
				shm->people_served += tables[table_num].current_people;
				break;
			case 1: // get bill
				if (tables[table_num].waiter != getpid()) {
					tables[table_num].waiter_answer = 2;
					logging << "wrong_waiter\tWaiter #" << getpid() << " is sleeping again." << endl;
					break;
					//continue; // sleep again and wake another waiter
				}
				tables[table_num].table_income += rand()%money_amount + 1;
				shm->overall_income += tables[table_num].table_income;
				tables[table_num].waiter = 0;
				tables[table_num].waiter_answer = 1;
				tables[table_num].available = true;
				shm->free_tables += 1;
				tables[table_num].current_people = 0;
				logging << "BILL\tWaiter #" << getpid() << " waiting for : " << st << endl;
				sleep(st);
				logging << "BILL\tWaiter #" << getpid() << " is getting payed by customers #" <<
					tables[table_num].customers	<< endl;
				break;
			case 2:
				shmdt(shm);
			    logging << "Waiter #" << getpid() << " finished successfully" << endl;
			    cout << "Waiter #" << getpid() << " finished successfully" << endl;
			    return 0;
			default:
				logging << "WRONG_CASE \tWaiter #" << getpid() << endl;
		}
		logging << "ANSWER\tWaiter #" << getpid() << " responding to customers #" <<
					tables[table_num].customers	<< endl;
		Sem_post(shm->waiter_answer,"waiter:: post waiter_answer ");
	}
    logging << "ERROR_Waiter #" << getpid() << " finished WRONG" << endl;
    cout << "ERROR_Waiter #" << getpid() << " finished WRONG" << endl;
	shmdt(shm);
	return 1;
}
