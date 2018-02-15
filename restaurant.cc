#include <iostream>
#include <sstream>
#include <fstream>
#include <cstdio>
#include <cerrno>
#include <cstdlib>
#include <ctime>

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "helper_funcs.h"

using namespace std;

int main(int argc ,char **argv) {
	cout << "Restaurant #" << getpid() << " is open!" << endl;
	int customers_num,sleep_time;
	char *config_file;
	/* Getting arguments */
	if (argc != 7) {
		cerr << "ERROR : Wrong arguments (" << argc << ") for restaurant.cpp!" << endl;
		cerr << "EXAMPLE : ./restaurant -n customers -l configfile -d time" << endl;
		exit(EXIT_FAILURE);
	}
	else {
		int i = 1;
		while (i < argc) {
			if (strncmp(argv[i],"-l",2) == 0)
				config_file = argv[i + 1];
			else if (strncmp(argv[i],"-d",2) == 0)
				sleep_time = atoi(argv[i + 1]);
			else if (strncmp(argv[i],"-n",2) == 0)
				customers_num = atoi(argv[i + 1]);
			i = i + 2;
		}
	}
	//cout << "Arguments : customers :" << customers_num <<
	//	",file :" << config_file << ",time :" << sleep_time << endl;

	/* Reading config file */
	ifstream config(config_file);
	int tables_num,bar_cap,waiters_num;
	if (config.is_open()) {
		string specifier,line;
		int number;
		while (getline(config, line)) {
			istringstream iss(line);
			if (!(iss >> specifier >> number))
				break;
			if (specifier == "tables_num")
				tables_num = number;
			else if (specifier == "bar_cap")
				bar_cap = number;
			else if (specifier == "waiters_num")
				waiters_num = number;
		}
	}
	else
		cerr << "Unable to open config file." << endl;
//	cout << "Restaurant has : " << tables_num << " tables ,a bar with capacity"
//	     << bar_cap << ",occupies " << waiters_num << " waiters and workload "<< workload << "%" << endl;

	srand((int)time(NULL)%getpid());

	/* Creating and initiallizing shared memory */
	key_t key = rand();
	Shared_mem_struct* shm;
	int shmid;
	shmid = shmget(key, sizeof(Shared_mem_struct) + tables_num * sizeof(Table)
					,IPC_CREAT | 0666);
	if (shmid < 0) {
		perror("shmget_restaurant");
		exit(EXIT_FAILURE);
	}
	shm = (Shared_mem_struct*) shmat(shmid, NULL, 0);
	if (shm == (Shared_mem_struct*) -1) {
		perror("shmat_restaurant");
		exit(EXIT_FAILURE);
	}
	shm->tarray = (Table*)((int *)shm + sizeof(Shared_mem_struct));
	/* Lock shared memory so no other process have access */
	Sem_init(shm->out_queue,1,"\tsem_init @ out_queue");
	Sem_init(shm->critical,1,"\tsem_init @ critical");
	Sem_init(shm->waiters,0,"\tsem_init @ waiters");
	Sem_init(shm->waiter_answer,0,"\tsem_init @ waiter_answer");
	Sem_init(shm->doorman,0,"\tsem_init @ doorman");
	Sem_init(shm->cust_waiting,0,"\tsem_init @ cust_waiting");
	//Sem_init(shm->bar,0,"\tsem_init @ bar");
	Sem_init(shm->bar2,0,"\tsem_init @ cust_waiting");
	Sem_init(shm->bar4,0,"\tsem_init @ cust_waiting");
	Sem_init(shm->bar6,0,"\tsem_init @ cust_waiting");
	Sem_init(shm->bar8,0,"\tsem_init @ cust_waiting");
	shm->waiters_num = waiters_num;
	shm->customers_num = customers_num;
	shm->child_synch = waiters_num + 1;
	shm->tables_num = tables_num;
	shm->bar_cap = bar_cap;
	shm->overall_income = 0;
	shm->people_in_bar = 0;
	shm->people_left = 0;
	shm->people_left_from_bar = 0;
	shm->people_served = 0;
	shm->free_tables = tables_num;
	shm->overall_income = 0;
	shm->doorman_answer = -1;
	shm->doorman_task = -1;
	shm->cur_group = -1;
	shm->cur_table = -1;
	shm->waiter_table = -1;
	shm->waiter_task = -1;
	for (int i = 0 ; i < tables_num ; i++) {
		shm->tarray[i].capacity = (rand()%4 + 1) * 2; //random number:[2,4,6,8]
		cout << "Table #" << i << "  /w cap " << shm->tarray[i].capacity << endl;
		shm->tarray[i].available = true;
		shm->tarray[i].waiter = -1;
	}

	/* Creating/forking waiters,customers & doorman */
	cout << "Forking doorman" << endl;
	pid_t doorman_pid = fork();
	if (doorman_pid < 0)
		perror("doorman_fork");
	else if (!doorman_pid) { // if doorman
		string program_name = Get_cwd("doorman");
		Call_exec(program_name.c_str(),'d',key,-1,5);
	}
	cout << "Forking waiters" << endl;
	for (int i = 0 ; i < waiters_num ; i++ ) {
		int money = rand()%MAX_BILL_AMOUNT + 1;
		pid_t waiterpid = fork();
		if (waiterpid < 0)
			perror("waiter_fork");
		else if (!waiterpid) {
			string program_name = Get_cwd("waiter");
			Call_exec(program_name.c_str(),'w',key,money,7);
		}
	}
	while(1) {
		if (!shm->child_synch) break;
	}
	cout << "Forking customers" << endl;
	for (int i = 0 ; i < customers_num ; i++ ) {
		int group = rand()%8 + 1;
		pid_t customerpid = fork();
		if (customerpid < 0)
			perror("customer_fork");
		else if (!customerpid) {
			Call_exec(Get_cwd("customer").c_str(),'c',key,group,7);
		}
	}
	sleep(sleep_time);
	Print_statistics(shm);
	/* Waiting all children to finish */
	int children_num = customers_num + waiters_num + 1;
	int status;
	pid_t child_pid;
	int doorman_exited = 0;
	while (children_num > 0) {
		child_pid = wait(&status);
		if (!WIFEXITED(status)) {
			errno = status;
			perror("child returned with error ");
			logging << "ERROR::Child #" << child_pid << " exit code : " << status << endl
					<< "Exit manually." << endl;
			cout << "ERROR::Child #" << child_pid << " exit code : " << status << endl
					<< "Exit manually." << endl;
			continue;
			if (status == (getpid() + 1)) {
				cout << "Restaurant :: is giving permission to doorman to leave" << endl;
				shm->doorman_task = 2;
				Sem_post(shm->doorman,"post doorman from restaurant ");
				doorman_exited = 1;
			} else {
				cout << "Restaurant :: is giving permission to a waiter to leave" << endl;
				shm->waiter_task = 2;
				Sem_post(shm->waiters,"post waiters from restaurant ");
				doorman_exited = 2; // a waiter exited
			}
		} else {
			cout << "#" << child_pid << " has finished with status " << status << endl;
		}
		children_num--;
		if ((children_num == waiters_num + 1) && (doorman_exited != 1)) {
			cout << "Restaurant :: is giving permission to doorman to leave" << endl;
			shm->doorman_task = 2;
			Sem_post(shm->doorman,"post doorman from restaurant ");
		} else if (children_num <= waiters_num) {
			cout << "Restaurant :: is giving permission to a waiter to leave" << endl;
			shm->waiter_task = 2;
			Sem_post(shm->waiters,"post waiters from restaurant ");
		}
	}

	shmdt(shm);
	/* Remove & detach shared memory */
	Sem_destroy_all(shm);
	if (shmctl(shmid, IPC_RMID, NULL) == -1)
		perror("SHM_Removal");
	else
		cout << "Shared memory removed correctly" << endl;

	cout << "Restaurant #" << getpid() << " is closed!" << endl;
    return EXIT_SUCCESS;
}
