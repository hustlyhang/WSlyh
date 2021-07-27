#include "threadpool.h"
#include <malloc.h>

threadpool_t *threadpool_create(int thread_count, int queue_size, int flags)
{
    threadpool_t *pool;
    int i;
    //(void) flags;
    //! 这儿用while(false)是因为为了方便中间出错误时可以直接跳过下面的步骤
    do
    {
        if(thread_count <= 0 || thread_count > MAX_THREADS || queue_size <= 0 || queue_size > MAX_QUEUE) {
            return NULL;
        }
    
        if((pool = (threadpool_t *)malloc(sizeof(threadpool_t))) == NULL) 
        {
            break;
        }
    
        /* Initialize */
        pool->thread_count = 0;
        pool->queue_size = queue_size;
        pool->head = pool->tail = pool->count = 0;
        pool->shutdown = pool->started = 0;
    
        /* Allocate thread and task queue */
        pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * thread_count);
        pool->queue = (threadpool_task_t *)malloc(sizeof(threadpool_task_t) * queue_size);
    
        /* Initialize mutex and conditional variable first */
        if((pthread_mutex_init(&(pool->lock), NULL) != 0) ||
           (pthread_cond_init(&(pool->notify), NULL) != 0) ||
           (pool->threads == NULL) ||
           (pool->queue == NULL)) 
        {
            break;
        }
    
        /* Start worker threads */
        for(i = 0; i < thread_count; i++) {
            if(pthread_create(&(pool->threads[i]), NULL, threadpool_thread, (void*)pool) != 0) 
            {
                threadpool_destroy(pool, 0);
                return NULL;
            }
            pool->thread_count++;
            pool->started++;
        }
        return pool;
    } while(false);
    
    //! 为什么pool会不为空？ 就是上面流程没走完，出问题了就可能建立线程池失败
    if (pool != NULL) 
    {
        threadpool_free(pool);
    }
    return NULL;
}

/*向线程池里面添加任务*/
int threadpool_add(threadpool_t *pool, void (*function)(void *), void *argument, int flags)
{
    //printf("add to thread pool !\n");
    int err = 0;
    int next;
    //(void) flags;
    if(pool == NULL || function == NULL)
    {
        return THREADPOOL_INVALID;
    }

    //? 向线程池里面添加任务时需要对线程加锁
    if(pthread_mutex_lock(&(pool->lock)) != 0)
    {
        return THREADPOOL_LOCK_FAILURE;
    }

    //? 任务加入位置
    next = (pool->tail + 1) % pool->queue_size;

    //! 为什么要用while（false）？
    do 
    {
        /* Are we full ? */
        if(pool->count == pool->queue_size) {
            err = THREADPOOL_QUEUE_FULL;
            break;
        }
        /* Are we shutting down ? */
        if(pool->shutdown) {
            err = THREADPOOL_SHUTDOWN;
            break;
        }
        /* Add task to queue */
        pool->queue[pool->tail].function = function;
        pool->queue[pool->tail].argument = argument;
        pool->tail = next;
        pool->count += 1;
        
        /* pthread_cond_broadcast */
        //? 任务添加完成，唤醒线程？
        if(pthread_cond_signal(&(pool->notify)) != 0) {
            err = THREADPOOL_LOCK_FAILURE;
            break;
        }
    } while(false);

    if(pthread_mutex_unlock(&pool->lock) != 0) {
        err = THREADPOOL_LOCK_FAILURE;
    }

    return err;
}


int threadpool_destroy(threadpool_t *pool, int flags)
{
    printf("Thread pool destroy !\n");
    int i, err = 0;

    if(pool == NULL)
    {
        return THREADPOOL_INVALID;
    }

    //? 销毁线程池时也需要加锁
    if(pthread_mutex_lock(&(pool->lock)) != 0) 
    {
        return THREADPOOL_LOCK_FAILURE;
    }

    //! 为什么要用while（false）？
    do 
    {
        /* Already shutting down */
        if(pool->shutdown) {
            err = THREADPOOL_SHUTDOWN;
            break;
        }

        pool->shutdown = (flags & THREADPOOL_GRACEFUL) ?
            graceful_shutdown : immediate_shutdown;

        /* Wake up all worker threads */
        if((pthread_cond_broadcast(&(pool->notify)) != 0) ||
           (pthread_mutex_unlock(&(pool->lock)) != 0)) {
            err = THREADPOOL_LOCK_FAILURE;
            break;
        }

        /* Join all worker thread */
        // 主线程等待所有子线程结束
        for(i = 0; i < pool->thread_count; ++i)
        {
            if(pthread_join(pool->threads[i], NULL) != 0)
            {
                err = THREADPOOL_THREAD_FAILURE;
            }
        }
    } while(false);

    /* Only if everything went well do we deallocate the pool */
    if(!err) 
    {
        threadpool_free(pool);
    }
    return err;
}

int threadpool_free(threadpool_t *pool)
{
    if(pool == NULL || pool->started > 0)
    {
        return -1;
    }

    /* Did we manage to allocate ? */
    if(pool->threads) 
    {
        free(pool->threads);
        free(pool->queue);
 
        /* Because we allocate pool->threads after initializing the
           mutex and condition variable, we're sure they're
           initialized. Let's lock the mutex just in case. */
        pthread_mutex_lock(&(pool->lock));
        pthread_mutex_destroy(&(pool->lock));
        pthread_cond_destroy(&(pool->notify));
    }
    free(pool);    
    return 0;
}


//? 每个线程执行的函数
static void *threadpool_thread(void *threadpool)
{
    threadpool_t *pool = (threadpool_t *)threadpool;
    threadpool_task_t task;

    for(;;)
    {
        //? 每个线程必须先将线程池锁上，然后从任务队列里面拿任务进行执行
        /* Lock must be taken to wait on conditional variable */
        pthread_mutex_lock(&(pool->lock));

        /* Wait on condition variable, check for spurious wakeups.
           When returning from pthread_cond_wait(), we own the lock. */
        while((pool->count == 0) && (!pool->shutdown)) 
        {
            //? 当线程池中的任务空的时候就需要阻塞线程，释放线程池的锁，让用户添加任务
            pthread_cond_wait(&(pool->notify), &(pool->lock));
        }


        if((pool->shutdown == immediate_shutdown) ||
           ((pool->shutdown == graceful_shutdown) &&
            (pool->count == 0)))
        {
            break;
        }

        /* Grab our task */
        task.function = pool->queue[pool->head].function;
        task.argument = pool->queue[pool->head].argument;
        pool->head = (pool->head + 1) % pool->queue_size;
        pool->count -= 1;

        /* Unlock */
        pthread_mutex_unlock(&(pool->lock));

        /* Get to work */
        //? 执行任务
        (*(task.function))(task.argument);
    }

    //? started参数指示当前空闲的线程数目？
    --pool->started;

    //? 是为了防止前面没有解锁的话
    pthread_mutex_unlock(&(pool->lock));
    //? 任务执行完毕，线程退出
    //! 线程退出后线程数是不是减少了？
    //? 线程会一直执行，如果需要摧毁线程池得话，才会走到这步
    pthread_exit(NULL);
    return(NULL);
}