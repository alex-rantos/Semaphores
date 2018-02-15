#include <iostream>
#include <sstream>
#include <fstream>
#include <cstdio>
#include <cerrno>
#include <cstdlib>

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ipc.h>

#include "helper_funcs.h"

using namespace std;

ofstream logging("logging.txt", std::ios::app);

int Max_cap_of_tables(Shared_mem_struct *shm,Table *tables) {
	int max = 0;
	for (int pos = 0 ; pos < shm->tables_num ; pos++) {
		if (tables[pos].capacity > max)
			max = tables[pos].capacity;
	}
	return max;
}
int Search_tables(Shared_mem_struct *shm,Table *tables) {
	int pos = -1;
	//cout << "free tables: " << shm->free_tables << endl;
	if (shm->free_tables > 0) { // search for a table suitable for the group
		//logging << "searching tables..." << endl;
		for (pos = 0 ; pos < shm->tables_num ; pos++) {
			//cout << "searching table # " << pos << "  avail" << tables[pos].available << " : " << shm->cur_group << endl;
			if ((tables[pos].available == true) && (tables[pos].capacity >= shm->cur_group)) {
				shm->cur_table = pos;
				logging << "\tDoorman allowed customers to sit @ #" << pos << " table" << endl;
				tables[pos].available = false;
				tables[pos].current_people += shm->cur_group;
				shm->doorman_answer = 0;
				shm->free_tables -= 1;
				break;
			}
		}
	}
		//logging << "No tables available..." << endl;
	return pos;
}


int Bar_calc(Shared_mem_struct *mem,int people_amount) {
	if (people_amount <= 2) {
		mem->b2 += 1;
	} else if (people_amount <= 4) {
		mem->b4 += 1;
	} else if (people_amount <= 6) {
		mem->b6 += 1;
	} else if (people_amount <= 8) {
		mem->b8 += 1;
	}
	return 0;
}


void Print_statistics(Shared_mem_struct *shm) {
	Table *tables = (Table*)((int*)shm + sizeof(Shared_mem_struct));
	int people_sum = 0;
	cout << "*******Restaurant_statistics*******" << endl;
	cout << "\tIncome per tables" << endl;
	for (int i = 0 ; i < shm->tables_num ; i++) {
		if (!tables[i].available)
			people_sum += tables[i].current_people;
		cout << "Table #" << i << " :: " << tables[i].table_income << endl;
	}
	cout << "People sitting at tables :: " << people_sum << endl;
	cout << "People currently sitting at bar :: " << shm->people_in_bar << endl;
	cout << "People left from bar :: " << shm->people_left_from_bar << endl;
	cout << "People that has been served :: " << shm->people_served << endl;
	cout << "People that left :: " << shm->people_left << endl;

}

char *Strduplicate(const char *source) {
	char *dest = (char *) malloc(strlen(source) + 1); // +1 = '\0'
	if (dest == NULL)
		return NULL;
	strcpy(dest, source);
	return dest;
}

string Get_cwd(string word) {
    char cwd[256];
    getcwd(cwd, sizeof(cwd));
    strcat(cwd,"/");
    strcat(cwd,word.c_str());
    string returnVal(cwd);
    return returnVal;
}

void Call_exec(const char *name,char c,key_t key,int value,int argc) {
	srand((int)time(NULL)%getpid());
	char *exec_array[8];
	int waiting_time = rand()%MAX_SLEEP_TIME;
	exec_array[0] = Strduplicate(name);
	exec_array[1] = (char *)malloc(2);
	exec_array[1] = (char *)"-d";
	asprintf(&(exec_array[2]), "%d", waiting_time);
	exec_array[3] = (char *)malloc(2);
	exec_array[3] = (char *)"-s";
	asprintf(&(exec_array[4]), "%d", key);
	if (argc > 5) {
		exec_array[5] = (char *)malloc(2);
		exec_array[5] = (char *)"-m";
		asprintf(&(exec_array[6]), "%d", value);
		//cout << exec_array[6] << " " << exec_array[7] << endl;
	}
	exec_array[argc] = (char *)malloc(1);
	exec_array[argc] = NULL;
	execvp(exec_array[0],exec_array);
}

void Sem_init(sem_t &sem,int val,const char *msg) {
	int pshared = 1; // If pshared is nonzero, then the semaphore is shared between processes
    if (sem_init(&sem,pshared,val) != 0) {
		perror(msg);
		exit(EXIT_FAILURE);
	}
}
void Sem_post(sem_t &sem,const char *msg) {
	if (sem_post(&sem) != 0) {
		perror(msg);
		exit(EXIT_FAILURE);
	}
}
void Sem_wait(sem_t &sem,const char *msg) {
	if (sem_wait(&sem) != 0) {
		perror(msg);
		exit(EXIT_FAILURE);
	}
}
void Sem_timedwait(Shared_mem_struct *shm,sem_t &sem,const char *errmsg,const char *msg,struct timespec &ts,int people_amount) {
	int s;
    s = sem_timedwait(&sem, &ts);
	if (s == -1) {
		if (errno == ETIMEDOUT) {
			logging << "BAR_EXIT:Customers waited too long & they leave" << endl;
			shm->people_left_from_bar += people_amount;
			shm->people_in_bar -= people_amount;
			if (people_amount <= 2) {
				shm->b2 -= 1;
			} else if (people_amount <= 4) {
				shm->b4 -= 1;
			} else if (people_amount <= 6) {
				shm->b6 -= 1;
			} else if (people_amount <= 8) {
				shm->b8 -= 1;
			}
			exit(EXIT_SUCCESS);
		}
		else {
			if (people_amount <= 2) {
				shm->b2 -= 1;
			} else if (people_amount <= 4) {
				shm->b4 -= 1;
			} else if (people_amount <= 6) {
				shm->b6 -= 1;
			} else if (people_amount <= 8) {
				shm->b8 -= 1;
			}
			perror("errmsg");
			exit(errno);
		}
	} else {
		if (people_amount <= 2) {
			shm->b2 -= 1;
		} else if (people_amount <= 4) {
			shm->b4 -= 1;
		} else if (people_amount <= 6) {
			shm->b6 -= 1;
		} else if (people_amount <= 8) {
			shm->b8 -= 1;
		}
		logging << "BAR\tCustomers (" << people_amount << ") #" << getpid()
				<< " from " <<  msg << " got awaken" << endl;
	}
}
void Sem_destroy_all(Shared_mem_struct *shm) {
	if (sem_destroy(&(shm->waiters)) != 0)
		handle_error("destroy waiters ");
	if (sem_destroy(&(shm->waiter_answer)) != 0)
		handle_error("destroy waiter_answer ");
	if (sem_destroy(&(shm->out_queue)) != 0)
		handle_error("destroy outqueue ");
	if (sem_destroy(&(shm->doorman)) != 0)
		handle_error("destroy doorman ");
	if (sem_destroy(&(shm->cust_waiting)) != 0)
		handle_error("destroy cust_waiting ");
	if (sem_destroy(&(shm->bar)) != 0)
		handle_error("destroy bar ");
	if (sem_destroy(&(shm->critical)) != 0)
		handle_error("destroy critical ");
	if (sem_destroy(&(shm->bar2)) != 0)
		handle_error("destroy bar2 ");
	if (sem_destroy(&(shm->bar4)) != 0)
		handle_error("destroy bar4 ");
	if (sem_destroy(&(shm->bar6)) != 0)
		handle_error("destroy bar6 ");
	if (sem_destroy(&(shm->bar8)) != 0)
		handle_error("destroy bar8 ");
}
