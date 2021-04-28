#include "helper.h"
#include "request.h"
#include "stat_process.h"

// Mutex and Condition Variables
pthread_mutex_t mutex;
pthread_cond_t empty;
pthread_cond_t full;
int numEmpty = 0;
int numFull = 0;
// The size of the bufferArray
int buffers = 0;
int listenfd;
//Tracks where to fill and where to consume
int fill_ptr = 0;
int consume_ptr  = 0;
int *bufferArray;

#define MAX_THREADS 32

// make a slot pointer array that fits up to 32 threads
slot_t* shm_slot_ptr;

// shm name
char *shm_name;

// 
// server.c: A very, very simple web server
//
// To run:
//  server <portnum (above 2000)>
//
// Repeatedly handles HTTP requests sent to this port number.
// Most of the work is done within routines written in request.c
//

// SIGINT HANDLER

void sigint_handler(int sig){
  printf("Captured signal SIGINT %d\n", sig);
	
	// unmap
	int ret = munmap(shm_slot_ptr, getpagesize());
	if (ret != 0) {
		fprintf(stderr, "Error unmapping shared memory region");
		exit(1);
	}

	// unlink
	ret = shm_unlink(shm_name);
	if (ret != 0) {
		fprintf(stderr, "Error unlinking shared memory regsion");
		exit(1);
	}

	free(bufferArray);

  exit(0);
}

// CS537: Parse the new arguments too
void getargs(int *port, int *threads, int *buffers, char **shm_name, int argc, char *argv[])
{
  if (argc != 5) {
    fprintf(stderr, "Usage: %s <port> <threads> <buffers> <shm_name>\n", argv[0]);
    exit(1);
  }
  if((*port = atoi(argv[1])) <= 2000) {
    fprintf(stderr, "Port number must be larger than 2000");
    exit(1);
  }
  if((*threads = atoi(argv[2])) <= 0){
    fprintf(stderr, "Threads must be a positive integer");
    exit(1);
  }
  if((*buffers = atoi(argv[3])) <= 0){
    fprintf(stderr, "Buffers must be a positive integer");
    exit(1);
  }
  *shm_name = argv[4];
}

int getEmpty(){
  int tmp = fill_ptr;
  // So that it is circular
  fill_ptr = (fill_ptr + 1) % (buffers);
  numEmpty--;
  return tmp;
}

void fillBuffer(int index){
  struct sockaddr_in clientaddr;
  int clientlen = sizeof(clientaddr);
  int connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);
  bufferArray[index] = connfd;
  return;
}

int getFull(){
  int tmp = consume_ptr;
  consume_ptr = (consume_ptr + 1) % (buffers);
  numFull--;
  return tmp;
}

int useBuffer(int index){
  int connfd = bufferArray[index];
  int ret = requestHandle(connfd);
  close(connfd);
  return ret;
}

void *producer(void *arg){

	int pagesize = getpagesize();

	// create the shared memeory region
	int shm_fd = shm_open(shm_name, O_RDWR | O_CREAT, 0660);
	if (shm_fd < 0) {
		fprintf(stderr, "Failed to create shared memeory region: %s", shm_name);
		exit(1);
	}

	// extend its size
	if (ftruncate(shm_fd, pagesize) != 0) {
    fprintf(stderr, "Failed to truncate shared memory object");
    exit(1);
	}

	// map memory into the processses address space
	void* shm_ptr = mmap(NULL, pagesize, PROT_WRITE, MAP_SHARED, shm_fd, 0);
	if (shm_ptr == MAP_FAILED) {
		fprintf(stderr, "Failed to map shared memory region to address space");
		exit(1);
	}

	// cast to an array
	shm_slot_ptr = (slot_t*) shm_ptr;

  while(1){
    pthread_mutex_lock(&mutex);
    while(numEmpty == 0){
      pthread_cond_wait(&empty, &mutex);
    }
    int tmp = getEmpty();
    pthread_mutex_unlock(&mutex);
    // TODO: Possible issue with fill_ptr not being right due to the locks here
    fillBuffer(tmp);
    pthread_mutex_lock(&mutex);
    numFull++;
    pthread_cond_signal(&full);
    pthread_mutex_unlock(&mutex);
  }
}

void *consumer(void *arg){
	int index = (intptr_t)arg;

  while(1){
    pthread_mutex_lock(&mutex);
    while(numFull == 0){
      pthread_cond_wait(&full, &mutex);
    }
    int tmp = getFull();
    pthread_mutex_unlock(&mutex);
    //TODO: Possible issue with getFull and useBuffer incrementing stuff that could be bad
    if (useBuffer(tmp)) {
			shm_slot_ptr[index].staticReq++;
		} else {
			shm_slot_ptr[index].dynamicReq++;
		}
    pthread_mutex_lock(&mutex);
    numEmpty++;
    pthread_cond_signal(&empty);
    pthread_mutex_unlock(&mutex);

		// write to shared mem region
		shm_slot_ptr[index].thread_id = pthread_self();
		shm_slot_ptr[index].totalReq++;
		
  }
}
int main(int argc, char *argv[])
{
  int port, threads;
  pthread_t main_thread;

  getargs(&port, &threads, &buffers, &shm_name, argc, argv);
  bufferArray = (int *)malloc(sizeof(int)*buffers);
  listenfd = Open_listenfd(port);
  // Check that the mutex and CVs are properly initialized
  if(pthread_mutex_init(&mutex, NULL) != 0){
    fprintf(stderr, "Mutex was not properly initialized");
  }
  if(pthread_cond_init(&empty, NULL) != 0){
    fprintf(stderr, "Empty condition variable not properly initialized");
  }
  if(pthread_cond_init(&full, NULL) != 0){
    fprintf(stderr, "Full condtion variable not properly initialized");
  }
  
	signal(SIGINT, sigint_handler);
  pthread_t consumers[threads];

  //Ititialize numEmpty to the number of buffer slots since they all initially have no work
  numEmpty = buffers;

  // 
  // CS537 (Part A): Create some threads...
  //
  pthread_create(&main_thread, NULL, producer, NULL);
  for(int i = 0; i < threads; i++){
    pthread_create(&consumers[i], NULL, consumer, (void*)(intptr_t)i);
  }

  pthread_join(main_thread, NULL);


  // Want to hand connfd over to worker thread and once that's done go to next thread
  // Instead of handling the connfd right away in the same thread like how this while loop is currently
  // doing this work
  // Push connfd to some queue structure then worker threads pull from that queue
  /*
  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);

    // 
    // CS537 (Part A): In general, don't handle the request in the main thread.
    // Save the relevant info in a buffer and have one of the worker threads 
    // do the work. Also let the worker thread close the connection.
    
    requestHandle(connfd);
    Close(connfd);
  }
  */
}
