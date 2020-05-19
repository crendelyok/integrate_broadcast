//---------------CLIENT-------------//

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


#define DEBUG 

#ifdef DEBUG
#define DBG
#else
#define DBG if(0)
#endif

int validatearg(int, char**);

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

#define MAX_BACKLOG 128
#define PORT        8080
#define UDP_BUF_LEN 64
#define TIMEOUT     20
#define MAX_CONS    16

int main() {
	//------------FIND WORKERS AKA SERVERS------------//
	//------------COLLECT CONNECTIONS-----------------//
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

        struct sockaddr_in client_addr = {
                .sin_family      = AF_INET,
                .sin_port        = htons(PORT),
                .sin_addr.s_addr = htonl(INADDR_BROADCAST)
        };
	struct sockaddr_in serv_addr = {};

        char udp_buf[UDP_BUF_LEN] = "";
	socklen_t addrlen = sizeof(struct sockaddr_in);
        char udp_throw[] = "THROW REQUEST";
        
	if (sendto(udp_fd, udp_throw, strlen(udp_throw) + 1, 0,
                   (struct sockaddr*) &client_addr, sizeof(client_addr)) < 0) {
                       perror("sendto\n");
                       exit(-1);
        }
        
	fcntl(udp_fd, F_SETFL, O_NONBLOCK);
	
	sleep(1);
	
	struct connection {
		int tcp_fd;
		struct sockaddr_in addr;	
		int num;
		int is_used;
	};

	struct connection* cons = calloc(MAX_CONS, sizeof(struct connection)); 

	int j = 0;
	while (recvfrom(udp_fd, udp_buf, UDP_BUF_LEN, 0,
                     (struct sockaddr*) &serv_addr, &addrlen) > 0) {
		
		printf("GOT BACK MESSAGE: %s\n", udp_buf);

		int tcp_fd = socket(AF_INET, SOCK_STREAM, 0);

		if (connect(tcp_fd, (struct sockaddr*) &serv_addr, 
			    sizeof(serv_addr)) < 0) {
			close(tcp_fd);
			continue;
		}
		
		int tcp_opt = 1;
		setsockopt(tcp_fd, SOL_SOCKET, SO_KEEPALIVE,
			   &tcp_opt, sizeof(tcp_opt));
		
		cons[j].addr    = serv_addr;
		cons[j].tcp_fd  = tcp_fd;
		cons[j].is_used = 1;
		recv(cons[j].tcp_fd, &(cons[j].num), sizeof(int), 0);

		++j;
        }

	int n_cons = j; 
        close(udp_fd);
	
	//
	
	printf("TOTAL NUMBER OF SERVERS %d\n", n_cons);
	if (n_cons == 0) {
		printf("No caluclations will be performed\n");
		exit(-1);
	}
	//------------MANAGING CONNECTIONS----------------//
	
	struct pollfd *fds = calloc(n_cons, sizeof(struct pollfd));

	int n_threads = 0;
	for (int i = 0; i < n_cons; ++i) {
		n_threads     += cons[i].num;
		fds[i].fd     =  cons[i].tcp_fd;
		fds[i].events =  POLLIN;
	}

	arg_t* args = calloc(n_threads, sizeof(arg_t));
	
	segment = (func_r - func_r) / n_threads;
	for (int i = 0; i < n_threads; ++i) {
		args[i].left  = func_l + i * segment;
		args[i].right = args[i].left + segment;
	}	

	j = 0;
	for (int i = 0; i < n_cons; ++i) {
		for (int k = 0; k < cons[i].num; ++k) {
			int garbage = 0;
			send(cons[i].tcp_fd, &garbage,       sizeof(garbage), 0);
			send(cons[i].tcp_fd, &args[j].left,  sizeof(double),  0);
			send(cons[i].tcp_fd, &args[j].right, sizeof(double),  0);
			
			++j;
		}
	}
	
	//calc ans
	double ans = 0;

	int finished = 0;
	while (finished < n_threads) {
		if (poll(fds, n_cons, -1) < 0) {
			perror("poll\n");
			exit(-1);
		}

		for (int k = 0; k < n_cons; ++k) {
			if (fds[k].revents & POLLIN) {
				int    tmp = -1;
				double this_ans = 0;
				if (recv(fds[k].fd, &this_ans, sizeof(double), 0) <= 0) {
					perror("recv\n");
					exit(-1);
				}
				if (recv(fds[k].fd, &tmp, sizeof(int), 0) <= 0) {
					perror("recv\n");
					exit(-1);
				}
				if (tmp < 0) 
					exit(-1);
				
				ans += this_ans;
				finished ++;
			}
			
			//poll error etc?
		}
	}
	printf("%.2lf\n", ans);
	fflush(stdout);

	free(fds);
	free(args);
	free(cons);

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
