
struct simple_delayed_work {
    int (*fn_work)(void *param);
    void *param;
    unsigned long delay;
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

static void *simple_worker_thread(void *param)
{
    struct simple_workqueue *wq = (struct simple_workqueue *)param;

    pthread_mutex_lock(&wq->mutex);
    wq->state |= WQ_STAT_LAUNCHED;
    pthread_cond_broadcast(&wq->cond);
    pthread_mutex_unlock(&wq->mutex);

    for ( ; ; ) {
        pthread_mutex_lock(&wq->mutex);
        if (wq->request & WQ_REQ_STOP) {
            wq->dwork = NULL;
            pthread_mutex_unlock(&wq->mutex);
            break;
        }
        if (wq->dwork) {
            ret = pthread_cond_timedwait(&wq->cond, &wq->mutex, &wq->wait_ts);
        }
        else {
            ret = pthread_cond_wait(&wq->cond, &wq->mutex);
        }
        switch (ret) {
            case ETIMEDOUT:
                wq->dwork->fn_work(wq->dwork->param);
                wq->dwork = NULL;
                break;
        }
        pthread_mutex_unlock(&wq->mutex);
    }

    return NULL;
}

struct simple_workqueue *create_simple_workqueue(void)
{
    int ret;
    struct simple_workqueue *wq;

    wq = calloc(1, sizeof(*wq));
    ASSERT(wq);

    pthread_mutex_init(&wq->mutex);
    pthread_cond_init(&wq->cond);

    ret = pthread_create(&wq->worker_thid, NULL, simple_worker_thread, wq);
    ASSERT(ret == ret);

    /* wait for launcing */
    pthread_mutex_lock(&wq->mutex);
    do {
        pthread_cond_wait(&wq->cond);
        if (wq->state & WQ_STAT_LAUNCHED)
            break;
    }
    pthread_mutex_unlock(&wq->mutex);

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

int schedule_delayed_work(struct simple_workqueue *wq,
        struct simple_work *work, unsigned long delay)
{
    pthread_mutex_lock(&wq->mutex);
    pthread_mutex_unlock(&wq->mutex);
}

int cancel_delayed_work(struct simple_delayed_work *dwork)
{
    pthread_mutex_lock(&dwork->queue->mutex);
    dwork->queue->work = NULL;
    pthread_cond_broadcast(&dwork->queue->cond);
    pthread_mutex_unlock(&dwork->queue->mutex);
    dwork->queue = NULL;
}

/*****************************************************************************/

int main(int argc, char **argv)
{
}

