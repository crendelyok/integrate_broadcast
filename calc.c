#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/sysinfo.h>
#include <errno.h>
#include <limits.h>
#include <math.h>

//#define DEBUG 

#ifdef DEBUG
#define DBG
#else
#define DBG if(0)
#endif

int validatearg(int, char**);
int get_n_proc();
void* thread_routine(void*);

const double func_dx = 1e-9;
const double func_l = 0;
const double func_r = 0.5;
double ans = 0;
double segment = 0;

pthread_mutex_t mutex;

int main(int argc, char* argv[]) {
	
	int n_threads = validatearg(argc, argv);
	int n_proc = get_n_proc();
	if (n_proc < n_threads) {
		printf("Too many threads (n_threads > n_cpus)\n");
		return 1;
	}

	DBG printf("Will use %d threads bro\n", n_threads);


	pthread_t* tids = calloc(n_threads, sizeof(pthread_t));
	if (tids == NULL) {
		perror("calloc\n");
		exit(1);
	}

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);


	int args[8] = {1, 2, 3, 4, 5, 6, 7, 8};

	//pthread_mutex_t mutex;
	pthread_mutex_init(&mutex, NULL);
	
	segment = (func_r - func_l) / n_threads;	

	for (long int i = 0; i < n_threads; ++i) {
		int ret_code = 0;
		ret_code = pthread_create(&tids[i], &attr, 
				thread_routine, &args[i]);
		if (ret_code != 0) {
			perror("pthread_create\n");
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
	}

	pthread_mutex_destroy(&mutex);
	pthread_attr_destroy(&attr);

	printf("%.2lf", ans);
	free(tids);

	DBG printf("main exited\n");
}

double func(double x) {
	return exp(x);
}

void* thread_routine(void* param) {
	int arg = *((int*)param);
		
	double left = func_l + (arg - 1) * segment;
	double right = left + segment;

	double cur_x = left;
	double this_ans = 0;
	while (cur_x <= right) {
		this_ans += func_dx * func(cur_x);	
		cur_x += func_dx;
	}

	pthread_mutex_lock(&mutex);
	ans += this_ans;
	DBG printf("thread %d : ans = %.2lf\n", arg, ans);
	DBG printf("left = %.2lf right = %.2lf \n\n", left, right);
	pthread_mutex_unlock(&mutex);

	pthread_exit(0);
}

int get_n_proc() {
	int n_proc = get_nprocs();
	//DBG printf("%d", n_proc);
	return n_proc;
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
