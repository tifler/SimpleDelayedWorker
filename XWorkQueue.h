#ifndef __XWORKQUEUE_H__
#define __XWORKQUEUE_H__

#include <pthread.h>
#include <sys/time.h>
#include "XList.h"

/*****************************************************************************/

#define toWork(w)               ((struct XWork *)(w))
#define toDelayedWork(w)        ((struct XDelayedWork *)(w))

/*****************************************************************************/

struct XWork {
    void (*func)(void *param);
    void *param;
    /* don't touch below */
    struct XList entry;
    struct XWorker *worker;
};

struct XDelayedWork {
    struct XWork work;
    struct timespec ts;
    struct XWorkQueue *queue;
};

/*****************************************************************************/

struct XWorkQueue *createXWorkQueue(void);
void destroyXWorkQueue(struct XWorkQueue *wq);
void initWork(struct XWork *work);
void initDelayedWork(struct XDelayedWork *dwork);
void scheduleWork(struct XWorkQueue *wq, struct XWork *work);
void scheduleDelayedWork(struct XWorkQueue *wq,
        struct XDelayedWork *dwork, unsigned long udelay);
void cancelWork(struct XWork *work);

#endif  /*__XWORKQUEUE_H__*/
