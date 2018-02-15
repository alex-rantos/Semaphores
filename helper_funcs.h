#ifndef _HELPER_FUNCS_H
#define _HELPER_FUNCS_H

#include <iostream>
#include <cerrno>
#include <semaphore.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <ctime>

#define MAX_SLEEP_TIME 2
#define MAX_WAITING_TIME_BAR 6
#define MAX_BILL_AMOUNT 250
#define handle_error(msg) \
    do { perror(msg); exit(EXIT_FAILURE); } while (0)

extern std::ofstream logging;

struct Table {
	pid_t waiter,customers;
	int capacity,current_people,table_income,
		waiter_answer; // 0 = order_ok , 1 = bill_ok , 2 = wrong waiter
	bool available;
};

struct Shared_mem_struct {
	int child_synch,waiters_num,customers_num,tables_num,
	bar_cap,overall_income,bar_state, // 0 = free table,1 = wait or leave
	people_in_bar,people_served,people_left,
	people_left_from_bar,doorman_task, // 0 = @queue,1 = @bar
	doorman_answer, // 0 = free table,1 = bar , 2 doorman_exit
	waiter_task, // 0 = order , 1 = bill , 2 waiter_exit
	free_tables,cur_group,cur_table,waiter_table;
	sem_t out_queue,doorman,cust_waiting,
	bar2,bar4,bar6,bar8,
	bar,waiters,waiter_answer,critical;
	int b2,b4,b6,b8;
	Table *tarray;
};

int Max_cap_of_tables(Shared_mem_struct *mem,Table *tables);
int Search_tables(Shared_mem_struct *mem,Table *tables);
int Bar_calc(Shared_mem_struct *mem,int cur_group);
void Print_statistics(Shared_mem_struct *mem);

std::string Get_cwd(std::string word);
char *Strduplicate(const char *source);
void Call_exec(const char *name,char c,key_t key,int value,int argc);
void Sem_init(sem_t &sem,int val,const char *msg);
void Sem_post(sem_t &sem,const char *msg);
void Sem_wait(sem_t &sem,const char *msg);
void Sem_timedwait(Shared_mem_struct *mem,sem_t &sem,const char *errmsg,const char *msg,struct timespec &ts,int people);
void Sem_destroy_all(Shared_mem_struct *mem);
#endif
