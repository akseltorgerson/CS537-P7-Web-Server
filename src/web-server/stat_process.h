#ifndef __STAT_PROCESS_H_
#define __STAT_PROCESS_H_

typedef struct slot {
	// thread id
	pthread_t thread_id;
	// total number of HTTP requests
	int totalReq;
	// number of static requests
	int staticReq;
	// dynamic requests
	int dynamicReq;
} slot_t;

#endif /* __STAT_PROCESS_H_ */