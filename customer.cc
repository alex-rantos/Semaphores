#include <sstream>
#include <fstream>
#include <cstdio>
#include <cstdlib>

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>

#include "helper_funcs.h"

using namespace std;

int main(int argc ,char **argv) {
    logging << "Customers #" << getpid() << " has started" << endl;
    int people_amount = 0, waiting_time = 0;
    key_t key;
	/* Getting arguments */
	if (argc != 7) {
		cerr << "ERROR : Wrong arguments (" << argc << ") for customer.cpp!" << endl;
		cerr << "EXAMPLE : ./customer -m people -d period -s shmid" << endl;
		exit(EXIT_FAILURE);
	}
	else {
		int i = 1;
		while (i < argc) {
			if (strncmp(argv[i],"-m",2) == 0)
				people_amount = atoi(argv[i + 1]);
			else if (strncmp(argv[i],"-s",2) == 0)
				key = (key_t) atoi(argv[i + 1]);
			else if (strncmp(argv[i],"-d",2) == 0)
				waiting_time = atoi(argv[i + 1]);
			i = i + 2;
		}
	}
	//logging << "Customer's argvs : people : " << people_amount << " shmid : " << key << " waiting_time " << waiting_time << endl;
	time_t t;
	srand((int)time(&t) % getpid());
	Shared_mem_struct* shm;
	int shmid = shmget(key, sizeof(Shared_mem_struct), 0666);
	if (shmid < 0) {
		perror("shmget_customer");
		exit(EXIT_FAILURE);
	}
	shm = (Shared_mem_struct*) shmat(shmid, NULL, 0);
	if (shm == (Shared_mem_struct*) -1) {
		perror("shmat_customer");
		exit(EXIT_FAILURE);
	}

	Table *tables = (Table*)((int*)shm + sizeof(Shared_mem_struct));
	int table_num = -1; // table number that this process is sitting at.

	/********************* DOORMAN_SECTION *********************/

	logging << "\tCustomers #" << getpid() << " waiting @ queue" << endl;
	Sem_wait(shm->out_queue,"customer waiting @ queue ");
	// wake doorman & inform
	shm->cur_group = people_amount;
	shm->doorman_task = 0;
	logging << "QUEUE\tCustomers (" << people_amount << ") #" << getpid() << " informing doorman @ queue" << endl;
	Sem_post(shm->doorman,"sem_post:customer_post @ doorman ");
	logging << "QUEUE\tCustomers (" << people_amount << ") #" << getpid() << " waiting doorman" << endl;
	Sem_wait(shm->cust_waiting,"sem_wait:customer waiting info from doorman ");
	logging << "QUEUE\tCustomers (" << people_amount << ") #" << getpid() << " getting doorman answer" << endl;

	switch(shm->doorman_answer) {
	case 1: // wait @ bar
		struct timespec ts;
		if (clock_gettime(CLOCK_REALTIME, &ts) == -1)
			handle_error("clock_gettime");
		ts.tv_sec += rand()%MAX_WAITING_TIME_BAR + 1;
		Sem_post(shm->doorman,"post customers served from doorman ");
		if (people_amount <= 2) {
			logging << "BAR\tCustomers (" << people_amount << ") #" << getpid() << " waiting at bar2" << endl;
			Sem_timedwait(shm,shm->bar2,"timedwait bar2","bar2",ts,people_amount);
		} else if (people_amount <= 4) {
			logging << "BAR\tCustomers (" << people_amount << ") #" << getpid() << " waiting at bar4" << endl;
			Sem_timedwait(shm,shm->bar4,"timedwait bar4","bar4",ts,people_amount);
		} else if (people_amount <= 6) {
			logging << "BAR\tCustomers (" << people_amount << ") #" << getpid() << " waiting at bar6" << endl;
			Sem_timedwait(shm,shm->bar6,"timedwait bar6","bar6",ts,people_amount);
		} else if (people_amount <= 8) {
			logging << "BAR\tCustomers (" << people_amount << ") #" << getpid() << " waiting at bar8" << endl;
			Sem_timedwait(shm,shm->bar8,"timedwait bar8","bar8",ts,people_amount);
		}
		shm->people_in_bar -= people_amount;
		logging << "\tCustomers (" << people_amount << ") #" << getpid() << " are sitting @ #" << shm->cur_table << " table" << endl;
		tables[shm->cur_table].available = false;
		tables[shm->cur_table].current_people = people_amount;
		tables[shm->cur_table].customers = getpid();
		table_num = shm->cur_table;
		break;
	case 0: // table is available
		logging << "\tCustomers (" << people_amount << ") #" << getpid() << " are sitting @ #" << shm->cur_table << " table" << endl;
		tables[shm->cur_table].available = false;
		tables[shm->cur_table].current_people = people_amount;
		tables[shm->cur_table].customers = getpid();
		table_num = shm->cur_table;
		Sem_post(shm->doorman,"post customers served from doorman ");
		break;
	case 2:
		logging << "EXITING:Customers (" << people_amount << ") #" << getpid() << " are leaving because restaurant is full" << endl;
		shm->people_left += people_amount;
		Sem_post(shm->doorman,"post customers served from doorman ");
		shmdt(shm);
		exit(EXIT_SUCCESS);
		break;
	default:
		logging << "CASE_WRONG \tCustomers (" << people_amount << ") #" << getpid() << " wrong doorman_answer" << endl;
	}

	/********************* WAITER_SECTION *********************/
	if (table_num == -1) {
		logging << "\tCustomers (" << people_amount << ") #" << getpid() << " have wrong table number" << endl;
		shmdt(shm);
		exit(EXIT_SUCCESS);
	}
	logging << "\tCustomers (" << people_amount << ") #" << getpid() << " waving @ a waiter" << endl;
	Sem_wait(shm->critical,"wait :: critical customer "); // inserting to CS
	shm->waiter_table = table_num;
	shm->waiter_task = 0; // order
	logging << "REQUEST\tCustomers (" << people_amount << ") #" << getpid() << " informing waiter." << endl;
	Sem_post(shm->waiters,"post::customer to waiter ");
	Sem_wait(shm->waiter_answer,"wait customer for waiter");
	if (tables[table_num].waiter_answer == 0) {
		logging << "ORDER\tCustomers (" << people_amount << ") #" << getpid() << " placed their order successfully." << endl;
	} else {
		logging << "W_ERROR\tCustomers (" << people_amount << ") #" << getpid() << " didn't order :: " << tables[table_num].waiter_answer << endl;
	}
	Sem_post(shm->critical,"post::critical customer ");
	int st = rand()%waiting_time;
	logging << "\tCustomers (" << people_amount << ") #" << getpid() << " sleeping for : " << st << endl;
	sleep(st); // eating/discussing
	Sem_wait(shm->critical,"wait :: critical customer "); // inserting to CS
	shm->waiter_table = table_num;
	shm->waiter_task = 1; // bill
	int pay_tries = 1;
	do { // wake waiters till you find the correct one.
		logging << "BILL\tCustomers (" << people_amount << ") #" << getpid()
				<< " wanna pay waiter #" << tables[table_num].waiter
				<< ".Try #" << pay_tries << endl;
		Sem_post(shm->waiters,"post::customer to waiter ");
		Sem_wait(shm->waiter_answer,"wait customer for waiter ");
		pay_tries++;
	} while((tables[table_num].waiter_answer) != 1);
	tables[table_num].waiter_answer = -1;
	Sem_post(shm->critical,"post::critical customer ");
    logging << "Customers (" << people_amount << ") #" << getpid() << " have payed and leaving the restaurant." << endl;
    cout << "Customers (" << people_amount << ") #" << getpid() << " have payed and leaving the restaurant." << endl;

	shmdt(shm);
    return EXIT_SUCCESS;
}
