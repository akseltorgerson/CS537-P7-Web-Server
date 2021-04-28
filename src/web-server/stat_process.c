#include "helper.h"
#include "request.h"
#include "stat_process.h"
#include "time.h"

int main(int argc, char *argv[]) {

	int sleeptime_ms;
	int num_threads;

	if (argc != 4) {
		fprintf(stderr, "Usage: %s [shm_name] [sleeptime_ms] [num_threads]\n", argv[0]);
		exit(1);
	}
	if ((sleeptime_ms = atoi(argv[2])) <= 0) {
		fprintf(stderr, "Sleeptime_ms must be larger than 0");
		exit(1);
	}
	if ((num_threads = atoi(argv[3])) <= 0 ) {
		fprintf(stderr, "Threads must be a positive integer");
		exit(1);
	}

	// program invoked by stat_process SHM_NAME
	char *shm_name = argv[1];

  // Shared memory reader
	int pagesize = getpagesize();

	// create shared memory region
	int shm_fd = shm_open(shm_name, O_RDWR, 0660);
	if (shm_fd < 0) {
		fprintf(stderr, "Failed to create shared memeory region: %s", shm_name);
		exit(1);
	}

	// memory map
	slot_t *shm_ptr = mmap(NULL, pagesize, PROT_READ, MAP_SHARED, shm_fd, 0);
	if (shm_ptr == MAP_FAILED) {
		fprintf(stderr, "Failed to map shared memory region to address space");
		exit(1);
	}

	slot_t* shm_slot_ptr = (slot_t*) shm_ptr;

	int secToSleep = sleeptime_ms / 1000;
	int msToSleep = sleeptime_ms % 1000;
	int nsToSleep = msToSleep * 1000000;

	struct timespec remaining, request = {secToSleep, nsToSleep};

	int it = 0;

	while (1) {
		fprintf(stdout, "\n%d\n", it);
		for (int i = 0; i < num_threads; i++) {
			fprintf(stdout, "%lu : %d %d %d\n", shm_slot_ptr[i].thread_id, shm_slot_ptr[i].totalReq, shm_slot_ptr[i].staticReq, shm_slot_ptr[i].dynamicReq);
		}
		it++;
		// read the shared memory region on wakeup of nanosleep
		nanosleep(&request, &remaining);
	}


	// unmap
	int ret = munmap(shm_ptr, pagesize);
	if (ret != 0) {
		fprintf(stderr, "Failed to unmap shared memory region");
		// exit(1);
	}

}