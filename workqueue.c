
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>

struct simple_delayed_work {
    void (*fn_work)(void *param);
    void *param;
    struct timespec wait_ts;
    struct simple_workqueue *queue;
};

struct simple_workqueue {
    pthread_t worker_thid;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    struct simple_delayed_work *dwork;
    unsigned int request;
    unsigned int state;
};

/*****************************************************************************/

#define WQ_REQ_STOP                 (0x8000)
#define WQ_REQ_SCHEDULE             (0x0001)

#define WQ_STAT_LAUNCHED            (0x0001)

static void *simple_worker_thread(void *param)
{
    int ret;
    struct simple_workqueue *wq = (struct simple_workqueue *)param;

    pthread_mutex_lock(&wq->mutex);
    wq->state |= WQ_STAT_LAUNCHED;
    pthread_cond_broadcast(&wq->cond);
    pthread_mutex_unlock(&wq->mutex);

    pthread_mutex_lock(&wq->mutex);
    do {
        if (wq->dwork) {
            printf("dwork = %p\n", wq->dwork);
            ret = pthread_cond_timedwait(&wq->cond, &wq->mutex, &wq->dwork->wait_ts);
        }
        else {
            printf("dwork = %p\n", wq->dwork);
            ret = pthread_cond_wait(&wq->cond, &wq->mutex);
        }

        printf("cond_wait ret = %d\n", ret);

        switch (ret) {
            case ETIMEDOUT:
                printf("==== TIMEDOUT ===\n");
                wq->dwork->fn_work(wq->dwork->param);
                wq->dwork = NULL;
                break;

            case EINVAL:
                break;

            default:
                break;
        }

        if (wq->request & WQ_REQ_STOP) {
            wq->dwork = NULL;
            break;
        }

    } while (!(wq->request & WQ_REQ_STOP));
    pthread_mutex_unlock(&wq->mutex);

    return NULL;
}

struct simple_workqueue *create_simple_workqueue(void)
{
    int ret;
    struct simple_workqueue *wq;

    wq = calloc(1, sizeof(*wq));
    assert(wq);

    pthread_mutex_init(&wq->mutex, NULL);
    pthread_cond_init(&wq->cond, NULL);

    ret = pthread_create(&wq->worker_thid, NULL, simple_worker_thread, wq);
    assert(ret == ret);

    /* wait for launcing */
    pthread_mutex_lock(&wq->mutex);
    do {
        pthread_cond_wait(&wq->cond, &wq->mutex);
    } while (!(wq->state & WQ_STAT_LAUNCHED));
    pthread_mutex_unlock(&wq->mutex);

    printf("created\n");

    return wq;
}

void destroy_simple_workqueue(struct simple_workqueue *wq)
{
    pthread_mutex_lock(&wq->mutex);
    wq->request |= WQ_REQ_STOP;
    pthread_cond_broadcast(&wq->cond);
    pthread_mutex_unlock(&wq->mutex);

    pthread_join(wq->worker_thid, NULL);
    free(wq);
}

#define USEC2SEC(us)                ((us) / 1000000)
#define USEC2NSEC(us)               ((us) * 1000)

void schedule_delayed_work(struct simple_workqueue *wq,
        struct simple_delayed_work *dwork, unsigned long udelay)
{
    struct timeval now;
    pthread_mutex_lock(&wq->mutex);
    gettimeofday(&now, NULL);
    dwork->wait_ts.tv_sec = now.tv_sec + USEC2SEC(udelay + now.tv_usec);
    dwork->wait_ts.tv_nsec = USEC2NSEC(udelay - now.tv_usec);
    dwork->queue = wq;
    wq->dwork = dwork;
    wq->request = WQ_REQ_SCHEDULE;
    pthread_cond_broadcast(&wq->cond);
    pthread_mutex_unlock(&wq->mutex);
}

void cancel_delayed_work(struct simple_delayed_work *dwork)
{
    pthread_mutex_lock(&dwork->queue->mutex);
    dwork->queue->dwork = NULL;
    pthread_cond_broadcast(&dwork->queue->cond);
    pthread_mutex_unlock(&dwork->queue->mutex);
    dwork->queue = NULL;
}

/*****************************************************************************/

static void test_callback(void *param)
{
    printf("==== callback with param %p ====\n", param);
}

int main(int argc, char **argv)
{
    struct simple_workqueue *wq;
    struct simple_delayed_work dwork;

    dwork.fn_work = test_callback;
    dwork.param = (void *)NULL;

    wq = create_simple_workqueue();
    schedule_delayed_work(wq, &dwork, 3000000);
    for ( ; ; )
        sleep (10);

    return 0;
}

