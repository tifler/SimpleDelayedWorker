#define DEBUG
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>

#include "XWorkQueue.h"
#include "XDebug.h"
#include "XList.h"

/*****************************************************************************/

#define WQ_REQ_STOP                 (0x8000)
#define WQ_REQ_SCHEDULE             (0x0001)
#define WQ_REQ_CANCEL               (0x0002)

#define WQ_STAT_LAUNCHED            (0x0001)

#define USEC2SEC(us)                ((us) / 1000000)
#define USEC2NSEC(us)               ((us) * 1000)

/*****************************************************************************/

struct XWorker {
    const char *name;
    pthread_t thid;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    unsigned int request;
    unsigned int state;
    struct XList works;
};

struct XWorkQueue {
    struct XWorker pender;
    struct XWorker worker;
};

/*****************************************************************************/
static void __scheduleWork(struct XWorker *worker, struct XWork *work);

/* should be called with wq's mutex */
static struct XWork *fetchWork(struct XList *works)
{
    struct XWork *work;

    ASSERT(works);

    if (XListEmpty(works))
        return NULL;

    work = XListEntry(works->next, struct XWork, entry);
    XListDel(&work->entry);

    return work;
}

static void *workerThread(void *param)
{
    struct XWork *work;
    struct XWorkQueue *wq = (struct XWorkQueue *)param;
    struct XWorker *worker = &wq->worker;

    pthread_mutex_lock(&worker->mutex);
    worker->state |= WQ_STAT_LAUNCHED;
    pthread_cond_broadcast(&worker->cond);
    pthread_mutex_unlock(&worker->mutex);

    pthread_mutex_lock(&worker->mutex);
    do {
        work = fetchWork(&worker->works);
        DBG("work = %p\n", work);

        if (work) {
            pthread_mutex_unlock(&worker->mutex);
            work->func(work->param);
            pthread_mutex_lock(&worker->mutex);
        }
        else {
            pthread_cond_wait(&worker->cond, &worker->mutex);
        }
    } while (!(worker->request & WQ_REQ_STOP));
    pthread_mutex_unlock(&worker->mutex);

    return NULL;
}

static struct XDelayedWork *getEarlierWork(
        struct XDelayedWork *dw1, struct XDelayedWork *dw2)
{
    if (dw1->ts.tv_sec < dw2->ts.tv_sec)
        return dw1;
    else if (dw1->ts.tv_sec > dw2->ts.tv_sec)
        return dw2;
    else if (dw1->ts.tv_nsec < dw2->ts.tv_nsec)
        return dw1;

    return dw2;
}

static struct XWork *peekEarliestWork(struct XList *works)
{
    struct XWork *earliest, *pos;

    ASSERT(works);

    if (XListEmpty(works))
        return NULL;

    earliest = NULL;
    XListForEachEntry(pos, works, entry) {
        if (!earliest) {
            earliest = pos;
            continue;
        }

        earliest = toWork(getEarlierWork(toDelayedWork(pos), toDelayedWork(earliest)));
    }

    return earliest;
}

static void *penderThread(void *param)
{
    int ret;
    struct XWork *work;
    struct XDelayedWork *dwork;
    struct XWorkQueue *wq = (struct XWorkQueue *)param;
    struct XWorker *pender = &wq->pender;

    pthread_mutex_lock(&pender->mutex);
    pender->state |= WQ_STAT_LAUNCHED;
    pthread_cond_broadcast(&pender->cond);
    pthread_mutex_unlock(&pender->mutex);

    pthread_mutex_lock(&pender->mutex);
    do {
        work = peekEarliestWork(&pender->works);
        dwork = toDelayedWork(work);
        DBG("dwork = %p\n", dwork);
        if (dwork) {
            ret = pthread_cond_timedwait(&pender->cond, &pender->mutex, &dwork->ts);
        }
        else {
            ret = pthread_cond_wait(&pender->cond, &pender->mutex);
        }

        DBG("Wait ret = %d\n", ret);

        switch (ret) {
            case ETIMEDOUT:
                DBG("TIMEDOUT. Move work-%p to worker\n", work);
                XListDel(&work->entry);
                scheduleWork(dwork->queue, work);
                break;

            default:
                break;
        }

    } while (!(pender->request & WQ_REQ_STOP));
    pthread_mutex_unlock(&pender->mutex);

    return NULL;
}


/*****************************************************************************/

static void createWorker(struct XWorker *worker,
        void *(*thread)(void *), const char *name, void *param)
{
    int ret;

    worker->name = name;
    pthread_mutex_init(&worker->mutex, NULL);
    pthread_cond_init(&worker->cond, NULL);
    XListInit(&worker->works);

    ret = pthread_create(&worker->thid, NULL, thread, param);
    ASSERT(ret == ret);

    /* wait for launcing */
    pthread_mutex_lock(&worker->mutex);
    do {
        pthread_cond_wait(&worker->cond, &worker->mutex);
    } while (!(worker->state & WQ_STAT_LAUNCHED));
    pthread_mutex_unlock(&worker->mutex);
    DBG("Created '%s' worker thread.\n", worker->name);
}

static void destroyWorker(struct XWorker *worker)
{
    pthread_mutex_lock(&worker->mutex);
    worker->request |= WQ_REQ_STOP;
    pthread_cond_broadcast(&worker->cond);
    pthread_mutex_unlock(&worker->mutex);
    pthread_join(worker->thid, NULL);
    DBG("Terminated '%s' worker thread.\n", worker->name);
}

struct XWorkQueue *createXWorkQueue(void)
{
    struct XWorkQueue *wq;
    struct XWorker *worker, *pender;

    wq = calloc(1, sizeof(*wq));
    ASSERT(wq);

    worker = &wq->worker;
    pender = &wq->pender;

    createWorker(worker, workerThread, "Worker", wq);
    createWorker(pender, penderThread, "Pender", wq);

    return wq;
}

void destroyXWorkQueue(struct XWorkQueue *wq)
{
    destroyWorker(&wq->pender);
    destroyWorker(&wq->worker);
    free(wq);
}

static void __scheduleWork(struct XWorker *worker, struct XWork *work)
{
    pthread_mutex_lock(&worker->mutex);
    XListAddTail(&work->entry, &worker->works);
    work->worker = worker;
    pthread_cond_broadcast(&worker->cond);
    pthread_mutex_unlock(&worker->mutex);
}

void scheduleWork(struct XWorkQueue *wq, struct XWork *work)
{
    struct XWorker *worker = &wq->worker;
    XListInit(&work->entry);
    __scheduleWork(worker, work);
}

void scheduleDelayedWork(
        struct XWorkQueue *wq, struct XDelayedWork *dwork, unsigned long udelay)
{
    struct timeval now;
    struct XWork *work = toWork(dwork);
    struct XWorker *pender = &wq->pender;

    TRACE_FUNC;

    gettimeofday(&now, NULL);
    dwork->ts.tv_sec = now.tv_sec + USEC2SEC(now.tv_usec + udelay);
    dwork->ts.tv_nsec = ((now.tv_usec + udelay) % 1000000) * 1000;
    dwork->queue = wq;

    DBG("%s(work-%p) will be moved to worker %ld.%09ld\n",
            __func__, work, dwork->ts.tv_sec, dwork->ts.tv_nsec);

    __scheduleWork(pender, work);
}

void cancelWork(struct XWork *work)
{
    struct XWorker *worker = work->worker;

    ASSERT(worker);

    pthread_mutex_lock(&worker->mutex);
    XListDel(&work->entry);
    pthread_cond_broadcast(&worker->cond);
    pthread_mutex_unlock(&worker->mutex);
    DBG("%s(work-%p) from worker-%s\n", __func__, work, worker->name);
}

/*****************************************************************************/

#if 0
void cancelDelayedWork(struct XDelayedWork *dwork)
{
    struct XWork *work = toWork(dwork);
    struct XWorker *pender = &work->worker;

    TRACE_FUNC;

    pthread_mutex_lock(&pender->mutex);
    pender->request = WQ_REQ_CANCEL;
    XListDel(&work->entry);
    pthread_cond_broadcast(&pender->cond);
    pthread_mutex_unlock(&pender->mutex);
}
#endif  /*0*/
