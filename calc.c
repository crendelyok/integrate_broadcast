#define _GNU_SOURCE

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/sysinfo.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <sched.h>

//#define DEBUG 

#ifdef DEBUG
#define DBG
#else
#define DBG if(0)
#endif

int validatearg(int, char**);
int get_n_proc();
void* thread_routine(void*);
void* fiction_routine(void*);


const double func_dx = 1e-9;
const double func_l = 0;
const double func_r = 0.5;
double segment = 0;

typedef struct routine_arg {
	double left;
	double right;
	double ans;
}arg_t;

arg_t* routine_arg_init(int);


int main(int argc, char* argv[]) {
	
	int n_threads = validatearg(argc, argv);
	int n_proc = get_nprocs();


	DBG printf("Will use %d threads bro\n", n_threads);

	int n_fict_thread = (n_threads > n_proc) ? n_threads : n_proc;
	
	pthread_t* tids = calloc(n_fict_thread, sizeof(pthread_t));
	if (tids == NULL) {
		perror("calloc\n");
		exit(1);
	}

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	//---by default---
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE); 
	//----------------

	segment = (func_r - func_l) / n_threads;	
	arg_t* args = routine_arg_init(n_threads);

	cpu_set_t cpu_set;

	for (int i = 0; i < n_threads; ++i) {
		CPU_ZERO(&cpu_set);
		CPU_SET(i % n_proc, &cpu_set); 

		int ret_code = 0;
		//0 on success
		ret_code = pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t),
				&cpu_set);
		
		if (ret_code != 0) {
			perror("pthread_attr_setaffinity\n");
			exit(1);
		}

		DBG printf("gonna create %d thread\n", i);

		ret_code = pthread_create(&tids[i], &attr, 
				thread_routine, &args[i]);
		if (ret_code != 0) {
			perror("pthread_create\n");
			exit(1);
		}
	}

	//sorry for this my friend
	for (int i = n_threads; i < n_proc; ++i) {
		CPU_ZERO(&cpu_set);
		CPU_SET(i, &cpu_set);

		pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t),
				&cpu_set);

		int ret_code = 0;
		ret_code = pthread_create(&tids[i], &attr,
				fiction_routine, NULL);
		if (ret_code != 0) {
			perror("fiction pthread_create\n");
			exit(1);
		}
	}

	for (int i = 0; i < n_threads; ++i) {
		int ret_code = 0;
		ret_code = pthread_join(tids[i], NULL);
		if (ret_code != 0) {
			perror("pthread_join\n");
			exit(1);
		}
		DBG printf("thread %d joined\n", i);
	}

	//calc ans
	double ans = 0;
	for (int i = 0; i < n_threads; ++i) {
		ans += args[i].ans;
	}
	printf("%.2lf", ans);
	fflush(stdout);

	pthread_attr_destroy(&attr);

	free(tids);
	free(args);

	DBG printf("main exit\n");
}

arg_t* routine_arg_init(int n_threads) {
	arg_t* args = calloc(n_threads, sizeof(arg_t));
	
	double x = func_l;
	for (int i = 0; i < n_threads; ++i) {
		args[i].left = x;
	      	x += segment;
		args[i].right = x;	
		DBG printf("%2d: [%.2lf, %.2lf]\n", i, args[i].left, args[i].right);
	}	
	return args;
}

double func(double x) {
	return exp(x);
}

void* fiction_routine(void* arg) {
	register int x = 0;
	while(1){x++;}
}

void* thread_routine(void* param) {
	register double cur_x = ((arg_t*)param) -> left;
	register double right = ((arg_t*)param) -> right;
	register double ans = 0;

	while (cur_x <= right) {
		ans += func_dx * func(cur_x);	
		cur_x += func_dx;
	}

	((arg_t*)param) -> ans = ans;

	DBG printf("routine: %.2lf\n", ans);
	return NULL;
	pthread_exit(0);
}

int validatearg(int argc, char* argv[]) {
	if (argc != 2) {
		printf("I need 1 argumet");
		exit(-1);
	}
	char* endptr = NULL;
	int val = strtol(argv[1], &endptr, 10);

	if (errno == ERANGE || errno == EINVAL || *endptr != '\0') {
		perror("strtol");
		exit(-1);
	}

	if (val < 1) {
		printf("val < 1");
		exit(-1);
	}
	return val;	
}
