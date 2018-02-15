#Compiler settings

CC = g++
CFLAGS = -c -Wall -g 
SRCS = restaurant.cc helper_funcs.cc
OBJS = $(SRCS:.cc=.o)
DEPS =
LIBS = -pthread -lrt
EXE = restaurant
TXT_FILES = logging.txt

all : $(EXE) customer doorman waiter
	@echo $(EXE) "HAS BEEN COMPILED"

$(EXE): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LIBS)

.cc.o:
	$(CC) $(CFLAGS) $< -o $@

customer: customer.o helper_funcs.o
	$(CC) $@.o helper_funcs.o -o $@ $(LIBS)

customer.o :
	$(CC) $(CFLAGS) customer.cc -o $@

doorman : doorman.o helper_funcs.o
	$(CC) $@.o helper_funcs.o -o $@ $(LIBS)

doorman.o :
	$(CC) $(CFLAGS) doorman.cc -o $@

waiter : waiter.o helper_funcs.o
	$(CC) $@.o helper_funcs.o -o $@ $(LIBS)

waiter.o :
	$(CC) $(CFLAGS) waiter.cc -o $@

test :
	g++ test.cc -pthread
	./a.out

clean :
	@echo "Removing files"
	rm $(EXE) $(TXT_FILES) customer doorman waiter *.o 
	./kill_ipcs.sh # macro that cleans up ipcs

rebuild :
	make clean
	make

send :
	tar -cvf AlexandrosRantos-Prj3.tar ./*
	chmod 755 AlexandrosRantos-Prj3.tar
	scp AlexandrosRantos-Prj3.tar sdi1300147@linux01.di.uoa.gr:/home/users/sdi1300147/

send_final :
	tar -cvf AlexandrosRantos-Prj3.tar ./*
	chmod 755 AlexandrosRantos-Prj3.tar
	scp AlexandrosRantos-Prj3.tar sdi1300147@linux01.di.uoa.gr:/home/users/k22/project3/sdi1300147/
