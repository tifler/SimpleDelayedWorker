
#include <stdio.h>
#include <unistd.h>
#include "XWorkQueue.h"
#include "XDebug.h"

static void print_time(void)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    INFO("%ld.%06ld\n", (long int)now.tv_sec, (long int)now.tv_usec);
}

static void test_callback(void *param)
{
    INFO("==== callback with param %p ====\n", param);
    print_time();
    sleep(1);
}

#if 1
int main(int argc, char **argv)
{
    int i;
    struct XWorkQueue *wq;
    struct XWork work;
    struct XDelayedWork dwork[3];

    wq = createXWorkQueue();

    for (i = 0; i < 3; i++) {
        dwork[i].work.func = test_callback;
        dwork[i].work.param = (void *)i;
        scheduleDelayedWork(wq, &dwork[i], (7 - i * 2) * 1000000);
    }

    cancelWork(toWork(&dwork[1]));

    sleep(10);

    destroyXWorkQueue(wq);

    return 0;
}
#else
int main(int argc, char **argv)
{
    int i;
    struct XWorkQueue *wq;
    struct XWork work[10];
    //struct XDelayedWork dwork;

    wq = createXWorkQueue();

    for (i = 0; i < 10; i++) {
        work[i].func = test_callback;
        work[i].param = (void *)i;
        scheduleWork(wq, &work[i]);
    }

    INFO("Queued all items.\n");

    //sleep(1);

    cancelWork(&work[0]);
    cancelWork(&work[1]);
    cancelWork(&work[9]);
    cancelWork(&work[8]);
    cancelWork(&work[7]);
    cancelWork(&work[3]);

#if 0
    scheduleDelayedWork(wq, &dwork, 2000000);
    sleep(1);
    cancelDelayedWork(&dwork);

    scheduleDelayedWork(wq, &dwork, 1000000);
    sleep(2);
    cancelDelayedWork(&dwork);
#endif  /*0*/

    sleep (10);

    destroyXWorkQueue(wq);

    return 0;
}
#endif  /*0*/
