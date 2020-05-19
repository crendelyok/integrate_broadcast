//-----------------------SERVER-----------------//

#define _GNU_SOURCE

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <sched.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <poll.h>


//------DEFINE DEBUG FOR DEBUG----------//
#define DEBUG 

#ifdef DEBUG
#define DBG
#else
#define DBG if(0)
#endif

const double func_dx = 1e-9;
const double func_l = 0;
const double func_r = 0.5;
double segment = 0;

typedef struct worker_arg {
	double left;
	double right;
	double ans;
}arg_t;

arg_t* workers_args_init(int);
int    validatearg      (int, char**);
void*  thread_routine   (void*);

#define MAX_BACKLOG 128
#define PORT        8080
#define UDP_BUF_LEN 64
#define TIMEOUT     20

int main(int argc, char* argv[]) {
	int n_threads = validatearg(argc, argv);

	//-----------START BROADCAST-----------//
	//----------IN BLOCKING MODE-----------//
	int udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (udp_fd == -1) {
		perror("socket\n");
		exit(-1);
	}

	int udp_opt = 1;
	if (setsockopt(udp_fd, SOL_SOCKET, SO_BROADCAST,
		       &(udp_opt), sizeof(udp_opt)) < 0) {
		perror("setcokopt\n");
		exit(-1);
	}

	struct sockaddr_in serv_addr = {
		.sin_family      = AF_INET,
		.sin_port        = htons(PORT),
		.sin_addr.s_addr = htonl(INADDR_ANY)
	};

	if (bind(udp_fd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0) {
		perror("bind\n");
		exit(-1);
	}


	//-----FOLOWING RECV DOESNT HAS TIMEOUT------//
	struct sockaddr_in client_addr = {};
	char udp_buf[UDP_BUF_LEN] = "";
	socklen_t addrlen = sizeof(struct sockaddr_in);
	if (recvfrom(udp_fd, udp_buf, UDP_BUF_LEN, 0,
		     (struct sockaddr*) &client_addr, &addrlen) < 0) {
		perror("recvfrom\n");
		exit(-1);
	}
	printf("REQUESTED WITH MESSAGE: %s\n", udp_buf);

	char udp_resp[] = "GOT YOUR REQUEST";
	if (sendto(udp_fd, udp_resp, strlen(udp_resp) + 1, 0,
		   (struct sockaddr*) &client_addr, sizeof(client_addr)) < 0) {
		       perror("sendto\n");
		       exit(-1);
	}
	close(udp_fd);
	//-----------------END BROADCAST--------------//

	DBG printf("BROADCAST FINISHED\nSTARTING TCP\n");
	
	//--------------START TCP CONNECTION----------//
	
	int tcp_fd = socket(AF_INET, SOCK_STREAM, 0);
	
	int tcp_opt = 1;
	if (setsockopt(tcp_fd, SOL_SOCKET, SO_REUSEPORT, //mb SO_REUSEADDR???????
		       &tcp_opt, sizeof(tcp_opt)) < 0) {
		perror("setsockopt\n");
		exit(-1);
	}

	fcntl(tcp_fd, F_SETFL, O_NONBLOCK);
	if (bind(tcp_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
		perror("bind\n");
		exit(-1);
	}
	
	listen(tcp_fd, MAX_BACKLOG);	

	struct pollfd tcp_fd_poll = {
		.fd = tcp_fd,
		.events = POLLIN
	};
	if (poll(&tcp_fd_poll, 1, TIMEOUT) <= 0) {
		perror("TIMEOUT ERROR, CLIENT DIED\n");
		exit (-1);
	}

	int client_fd = accept(tcp_fd, (struct sockaddr*)&client_addr, &addrlen);
	if (client_fd < 0) {         //check "man(2) accept" for errno 
		perror("accept\n");
		exit(-1);
	}
	close(tcp_fd);
	//---------------END TCP CONNECTION---------//
	//--------------RESULT IS client_fd---------//



	//--------------THREADING TIME--------------//

	arg_t*     args    = calloc(n_threads, sizeof(arg_t)); 
	pthread_t* threads = calloc(n_threads, sizeof(pthread_t));

	send(client_fd, &n_threads, sizeof(n_threads), 0);
	//!!check for error!!
	
	char garbage = '0';
	do {
		if (recv(client_fd, &garbage, sizeof(garbage), MSG_PEEK) <= 0)
			break;

		for (int i = 0; i < n_threads; ++i) {
			if (recv(client_fd, &garbage, sizeof(garbage),
				 MSG_PEEK | MSG_DONTWAIT) <= 0)
				break;
			recv(client_fd, &(args[i].left),  sizeof(double), 0);
			recv(client_fd, &(args[i].right), sizeof(double), 0);

			if (pthread_create(&(threads[i]), NULL,
			    thread_routine, (void *) (&args[i])) != 0) {
				perror("pthread_create\n");
				exit(-1);
			}
		}
		
		for (int i = 0; i < n_threads; ++i) {
			if (pthread_join(threads[i], NULL) != 0) {
				perror("pthread_join\n");
				exit(-1);
			}
			send(client_fd, &(args[i].ans), sizeof(double), 0);
		}
	} while (recv(client_fd, &garbage, 
		 sizeof(garbage), MSG_DONTWAIT | MSG_PEEK) != 0);

	close(client_fd);
	free (args);
	free (threads);

	DBG printf("main exit\n");
	return 0;
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

double func(double x) {
        return exp(x);
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

