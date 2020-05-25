//---------------CLIENT AKA CALC-------------//

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


//#define DEBUG 

#ifdef DEBUG
#define DBG
#else
#define DBG if(0)
#endif

#include "consts"

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
		
                       perror("sendto udp\n");
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
		
		printf("Connection established: %s\n", udp_buf);

		int tcp_fd = socket(AF_INET, SOCK_STREAM, 0);

		if (connect(tcp_fd, (struct sockaddr*) &serv_addr, 
			    sizeof(serv_addr)) < 0) {
			close(tcp_fd);
			continue;
		}
		
		
		int tcp_opt = 1;
		int keepcnt_opt = 1;
		int keepidle_opt = 1;
		int keepintvl_opt = 1;
		setsockopt(tcp_fd, SOL_SOCKET,  SO_KEEPALIVE,  &tcp_opt,       sizeof(tcp_opt));
		setsockopt(tcp_fd, IPPROTO_TCP, TCP_KEEPCNT,   &keepcnt_opt,   sizeof(tcp_opt));
		setsockopt(tcp_fd, IPPROTO_TCP, TCP_KEEPIDLE,  &keepidle_opt,  sizeof(tcp_opt));
		setsockopt(tcp_fd, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl_opt, sizeof(tcp_opt));
		
		cons[j].addr    = serv_addr;
		cons[j].tcp_fd  = tcp_fd;
		cons[j].is_used = 1;
		recv(cons[j].tcp_fd, &(cons[j].num), sizeof(int), 0);
		printf("server asked for %d threads\n", (cons[j].num));

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
	
	segment = (func_r - func_l) / n_threads;
	for (int i = 0; i < n_threads; ++i) {
		args[i].left  = func_l + i * segment;
		args[i].right = args[i].left + segment;
	}	

	j = 0;
	for (int i = 0; i < n_cons; ++i) {
		for (int k = 0; k < cons[i].num; ++k) {
			int garbage = k;
			send(cons[i].tcp_fd, &garbage,       sizeof(garbage), 0);
			send(cons[i].tcp_fd, &args[j].left,  sizeof(double),  0);
			send(cons[i].tcp_fd, &args[j].right, sizeof(double),  0);
			printf("Sent: {%d} [%.2lf; %.2lf]\n", garbage, args[j].left, args[j].right);
			++j;
		}
	}
	
	//calc ans
	double ans = 0;

	int finished = 0;
	int working = n_cons;
	while (finished < n_threads && working > 0) {
		if (poll(fds, n_cons, -1) < 0) {
			perror("poll\n");
			exit(-1);
		}
			DBG printf("loop\n");

		for (int k = 0; k < n_cons; ++k) {
			if (cons[k].is_used == 0)
				continue;

			if (fds[k].revents & POLLIN) {
				int    tmp = 0;
				double this_ans = 0;
				if (recv(fds[k].fd, &tmp, sizeof(int), 0) <= 0) {
					working --;
					cons[k].is_used = 0;
					continue;
					perror("recv tmp\n");
					exit(-1);
				}
				if (recv(fds[k].fd, &this_ans, sizeof(double), 0) <= 0) {
					working --;
					cons[k].is_used = 0;
					continue;
					perror("recv ans\n");
					exit(-1);
				}
				
				ans += this_ans;
				DBG printf("RECIEVED %d with answer {%.2lf}\n", tmp, this_ans);
				finished ++;
			}
			
			//connection dead?
			if ((fds[k].revents & POLLHUP) || (fds[k].revents & POLLRDHUP) ||
			    (fds[k].revents & POLLERR))
				finished ++;
		}
	}
	if (working <= 0) {
		printf("worker(s) dead\n");
		return -1;
	}
	printf("\n--------------\n%.2lf\n", ans);
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
