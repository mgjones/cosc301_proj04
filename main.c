//
//  By Mariah Jones &
//  Nolan Gonzalez
//
//
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
	// for weblog purposes
	struct sockaddr_in client_address;
	struct work_queue_item *next;
};


// globals //
struct work_queue_item *head = NULL;
struct work_queue_item *tail = NULL;
pthread_mutex_t workmutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t loglock = PTHREAD_MUTEX_INITIALIZER;
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

//	worker and queue functions	//

void additem(int sock, struct sockaddr_in client_address){
	struct work_queue_item *new_item = malloc(sizeof(struct work_queue_item));
	new_item->client_address = client_address;
	new_item->sock = sock;

	if (head == NULL){ // no nodes
		new_item->next = NULL;
		head = new_item; 
		tail = head;
	}else{ // some nodes
		new_item->next = head;
		head = new_item;
	}
	return;
}

int removeitem(){
	struct work_queue_item *traverse = head;
	int i = 0;
	int retsock;
	if (head == NULL){
		return 0;
	}else if (head->next == NULL){ // only 1 node
		retsock = tail->sock;
		free(tail);
		// being cautious because
		// they're global
		tail = NULL;
		head = NULL;
		count--;
		return retsock;
	}else{
		for (; i < count - 1; i++){
			traverse = traverse->next;
		}
		// tail points to node before last node
		tail = traverse;
		int retsock = (traverse + 1)->sock;
		free(traverse+1);
		traverse->next = NULL;
		count--;
		return retsock;
	}
	return 0;
}


void *worker(void* arg){
	while(1){
		pthread_mutex_lock(&workmutex);
		while(count == 0){
			pthread_cond_wait(&workcond, &workmutex);
		}
		struct sockaddr_in client_address = tail->client_address;
		int sock = removeitem();	
		pthread_mutex_unlock(&workmutex);

		// respond to request //
		char reqbuffer[1024];
		int x = getrequest(sock, reqbuffer,1024);
		struct stat buffer;
		if(x == -1){
			printf("Error: could not get request\n");
			return NULL;
		}

		int err = stat(reqbuffer, &buffer);
		if(err == -1){
			printf("file %s doesn't exist\n",reqbuffer);
			senddata(sock, HTTP_404, sizeof(HTTP_404));
			return NULL;
		}
		senddata(sock, HTTP_200, err);	

		int file = open(reqbuffer, O_RDONLY); //S_IRUSR - no workie
		int size;
		while(1){
			// sending some arbitrary size, 1024,
			// because it probably doesn't matter in
			// how many increments it reads in the file
			size = read(file, &buffer, 1024);
			if(size == 0)
				break;
			senddata(sock, HTTP_200, size);
		}

		// printing to log //
		pthread_mutex_lock(&loglock);
		FILE *weblog = fopen("weblog.txt", "w");
		time_t now = time(NULL);
		fprintf(weblog, "Got connection from %s:%d at %s\n", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port), ctime(&now));
		pthread_mutex_unlock(&loglock);

		close(file);
	}
	return NULL;
}

//	//	//	//	//	//	//	//	//


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


			pthread_mutex_lock(&workmutex);
			additem(new_sock,client_address);
			count++;
			pthread_cond_signal(&workcond);
			pthread_mutex_unlock(&workmutex);
			

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
