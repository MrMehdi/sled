// os_unix
// The platform specific code for UNIX-likes.

#include "../types.h"
#include "../oscore.h"
#include "../main.h"
#include "../timers.h"
#include <sys/time.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>

#if defined(__FreeBSD__) || defined(__OpenBSD__)
#include <pthread_np.h>
#endif


// Main method.
int main(int argc, char** argv) {
	return sled_main(argc, argv);
}

// -- event
typedef struct {
	int send;
	int recv;
} oscore_event_i;

int oscore_event_wait_until(oscore_event ev, ulong desired_usec) {
	ulong tnow = udate();
	
	// DEBUG TIME
	printf("\tos_unix\t\t\t\t\t\t\tnow=%08lx\tdesired=%08lx\n", tnow, desired_usec);
	if (tnow >= desired_usec || (desired_usec > 0xA0000000 && tnow < 0x50000000) )
		return tnow;
		
	ulong sleeptime = desired_usec - tnow;
	
	// DEBUG TIME
	printf("\tos_unix\t\t\t\t\t\t\tsleeptime:0x%08lx = %ld us\n", sleeptime, sleeptime);
	
	oscore_event_i * oei = (oscore_event_i *) ev;
	struct timeval timeout;
	timeout.tv_sec = sleeptime / 1000000;
	timeout.tv_usec = sleeptime % 1000000;
	fd_set set;
	FD_ZERO(&set);
	FD_SET(oei->recv, &set);
	if (select(FD_SETSIZE, &set, NULL, NULL, &timeout)) {
		char buf[512];
		read(oei->recv, buf, 512);
		return 1; // we got an interrupt
	}
	return 0; // timeout
}

void oscore_event_signal(oscore_event ev) {
	oscore_event_i * oei = (oscore_event_i *) ev;
	char discard = 0;
	write(oei->send, &discard, 1);
}

oscore_event oscore_event_new(void) {
	oscore_event_i * oei = malloc(sizeof(oscore_event_i));
	int fd[2];
	assert(oei);
	pipe(fd);
	oei->recv = fd[0];
	oei->send = fd[1];
	return oei;
}

void oscore_event_free(oscore_event ev) {
	oscore_event_i * oei = (oscore_event_i *) ev;
	close(oei->send);
	close(oei->recv);
	free(oei);
}

ulong oscore_starttime = 0;

// Time keeping.
ulong oscore_udate(void) {
	struct timeval tv;
	if (gettimeofday(&tv, NULL) == -1) {
		printf("Failed to get the time???\n");
		exit(1);
	}
	// DEBUG TIME!
	if( oscore_starttime == 0) {
		oscore_starttime = 0xFFFFFFFF - (ulong)(T_SECOND * tv.tv_sec + tv.tv_usec) - 0x00104000;
		printf("STARTTIME: %08lx\n", oscore_starttime);
	}
	return T_SECOND * tv.tv_sec + tv.tv_usec + oscore_starttime;
	//NORMAL CODE: return T_SECOND * tv.tv_sec + tv.tv_usec;
}

// Threading
oscore_task oscore_task_create(char* name, oscore_task_function func, void* ctx) {
	pthread_t* thread = calloc(sizeof(pthread_t), 1);
	pthread_create(thread, NULL, func, ctx);

#if defined(__linux__) || defined(__NetBSD__)
        pthread_setname_np(*thread, name);
#elif defined(__FreeBSD__) || defined(__OpenBSD__)
        pthread_set_name_np(*thread, name);
#endif
        return 0;
}

void oscore_task_yield(void) {
	// nothing.
};

void oscore_task_exit(int status) {
	pthread_exit(&status);
};

int oscore_task_join(oscore_task task) {
	int* retval = calloc(sizeof(int), 1);
	if (pthread_join(task, (void*) retval)) {
		free(task);
		free(retval);
		return -1;
	}
	free(task);
	int ret = *retval;
	free(retval);
	return ret;
};


// -- mutex
oscore_mutex oscore_mutex_new(void) {
	pthread_mutex_t * mutex = malloc(sizeof(pthread_mutex_t));
	assert(mutex);
	assert(!pthread_mutex_init(mutex, NULL));
	return mutex;
}

void oscore_mutex_lock(oscore_mutex m) {
	pthread_mutex_t * mutex = m;
	pthread_mutex_lock(mutex);
}

void oscore_mutex_unlock(oscore_mutex m) {
	pthread_mutex_t * mutex = m;
	pthread_mutex_unlock(mutex);
}

void oscore_mutex_free(oscore_mutex m) {
	pthread_mutex_t * mutex = m;
	pthread_mutex_destroy(mutex);
	free(mutex);
}
