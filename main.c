/*
 * Nolan Gonzalez and Mariah Jones
 * November 20, 2013
 *
*/

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <arpa/inet.h>

#include "network.h"


struct work_queue_item{
	int sock;
	struct work_queue_item *next;
};


// globals //
struct work_queue_item *head = NULL;
struct work_queue_item *tail = NULL;
pthread_mutex_t workmute = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t workcond = PTHREAD_COND_INITIALIZER;
int count = 0;



// global variable; can't be avoided because
// of asynchronous signal interaction
int still_running = TRUE;
void signal_handler(int sig) {
    still_running = FALSE;
}


void usage(const char *progname) {
    fprintf(stderr, "usage: %s [-p port] [-t numthreads]\n", progname);
    fprintf(stderr, "\tport number defaults to 3000 if not specified.\n");
    fprintf(stderr, "\tnumber of threads is 1 by default.\n");
    exit(0);
}

////////// ADD FUNCTION ////////// compiled -- not yet tested

void additem(int sock){

	if (head == NULL){
		struct work_queue_item *new_item = malloc(sizeof(struct work_queue_item));
		head = new_item; 
		new_item->sock = sock;
		new_item->next = NULL;
		head->next = NULL; 
	}else{
		struct work_queue_item *new_item = malloc(sizeof(struct work_queue_item));
		new_item->sock = sock;
		new_item->next = head; 
		head = new_item; 
	}
	return;
}

////////// REMOVE FUNCTION ////////// compiled -- not yet tested

int removeitem(){

	struct work_queue_item *traverse = head;
	int i = 0;

	if (tail == NULL){
		return 0;
	// there is only one item in the linked list
	}else if (head->next == NULL){ 
		return tail->sock;
	}else{
		
		for (; i < count - 1; i++){
			traverse = traverse->next;
		}
		tail = traverse;
		return (traverse + i)->sock; 
	}
	//free node that was removed 
	return 0;
}

void *worker(void* arg){
	return NULL;
}

void runserver(int numthreads, unsigned short serverport) {
    //////////////////////////////////////////////////

    // create your pool of threads here

    //////////////////////////////////////////////////

    pthread_t threads[numthreads];
	int i = 0;
	while(i < numthreads){
		pthread_create(&threads[i],NULL,worker,NULL);
		i++;
	}
	
    //////////////////////////////////////////////////
    int main_socket = prepare_server_socket(serverport);
    if (main_socket < 0) {
        exit(-1);
    }
    signal(SIGINT, signal_handler);

    struct sockaddr_in client_address;
    socklen_t addr_len;

    fprintf(stderr, "Server listening on port %d.  Going into request loop.\n", serverport);
    while (still_running) {
        struct pollfd pfd = {main_socket, POLLIN};
        int prv = poll(&pfd, 1, 10000);

        if (prv == 0) {
            continue;
        } else if (prv < 0) {
            PRINT_ERROR("poll");
            still_running = FALSE;
            continue;
        }
        
        addr_len = sizeof(client_address);
        memset(&client_address, 0, addr_len);

        int new_sock = accept(main_socket, (struct sockaddr *)&client_address, &addr_len);
        if (new_sock > 0) {
            
            time_t now = time(NULL);
            fprintf(stderr, "Got connection from %s:%d at %s\n", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port), ctime(&now));

           ////////////////////////////////////////////////////////
           /* You got a new connection.  Hand the connection off
            * to one of the threads in the pool to process the
            * request.
            *
            * Don't forget to close the socket (in the worker thread)
            * when you're done.
            */
           ////////////////////////////////////////////////////////

			// write code here //

		   ////////////////////////////////////////////////////////

        }
    }
    fprintf(stderr, "Server shutting down.\n");
        
    close(main_socket);
}


int main(int argc, char **argv) {
    unsigned short port = 3000;
    int num_threads = 1;

    int c;
    while (-1 != (c = getopt(argc, argv, "hp:t:"))) {
        switch(c) {
            case 'p':
                port = atoi(optarg);
                if (port < 1024) {
                    usage(argv[0]);
                }
                break;

            case 't':
                num_threads = atoi(optarg);
                if (num_threads < 1) {
                    usage(argv[0]);
                }
                break;
            case 'h':
            default:
                usage(argv[0]);
                break;
        }
    }

    runserver(num_threads, port);
    
    fprintf(stderr, "Server done.\n");
    exit(0);
}
