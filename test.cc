#include <unistd.h>
#include <dirent.h>
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <iostream>
#include <cerrno>
#include <semaphore.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>

struct Table {
	pid_t waiter;
	int capacity,amount,sum;
	bool available;
};

struct Shared_mem_struct {
	int value;
	sem_t sem;
	Table *array;
};

using namespace std;

int main(int argc ,char **argv) {
	cout << "TEST #" << getpid() << " is running!" << endl;
	srand(time(NULL));

	int tables_num = 15;
	cout << "Amount of tables : " << tables_num << endl;
	key_t key = ftok("./test.cc",rand()%60);
	//key_t key = 65536;
	cout << " KEY : " << key << endl;
	Shared_mem_struct* shm;
	int shmid;
	shmid = shmget(key,sizeof(Shared_mem_struct) + tables_num * sizeof(Table), IPC_CREAT | 0666);
	if (shmid < 0) {
		perror("shmget");
		exit(EXIT_FAILURE);
	}
	shm = (Shared_mem_struct*) shmat(shmid, NULL, 0);
	if (shm == (Shared_mem_struct*) -1) {
		perror("shmat");
		exit(EXIT_FAILURE);
	}
	if ((shmid = shmget(key,tables_num * sizeof(Table),IPC_CREAT | 0666)) < 0) {
		perror("shmget(2)");
		exit(errno);
	}
	shm->array = (Table*)shmat(shmid,NULL,0);
	if ((shm->array) == (Table*) -1) {
		perror("shmat(2)");
		exit(EXIT_FAILURE);
	}
	shm->array = new Table[tables_num];

	sem_init(&shm->sem,1,1);

	shmdt(shm);
	if (shmctl(shmid, IPC_RMID, NULL) == -1)
		perror("SHM_Removal");
	else
		cout << "Shared memory removed correctly" << endl;
    return EXIT_SUCCESS;
}
