#ifndef LST_TIMER
#define LST_TIMER

#include <netinet/in.h>
#include <string>
#include "../log/log.h"

class util_timer;

struct client_data 
{
    sockaddr_in address;
    int sockfd;
    util_timer* timer;
};

class util_timer 
{
public:
    util_timer(): prev(NULL), next(NULL) {}
public:
    time_t expire;
    void (*cb_func)(client_data*);
    client_data *user_data;
    util_timer *prev, *next;
};


class sort_timer_list 
{
public:
    sort_timer_list():head(NULL), tail(NULL) {}
    ~sort_timer_list()
    {
        util_timer *tmp = head;
        while (tmp) 
        {
            head = head->next;
            delete tmp;
            tmp = head;
        }
    }

    void add_timer(util_timer *timer) 
    {
        if (!timer) return;
        if (!head) 
        {
            head = tail = timer;
            return;
        }
        if (timer->expire < head->expire) 
        {
            timer->next = head;
            head->prev = timer;
            head = timer;
            return;
        }
        add_timer(timer, head);
    }

    void adjust_timer(util_timer *timer) 
    {
        if (!timer) return;

        util_timer *tmp = timer->next;

        // 如果下一个为空，或者下一个过期时间小于现在这个，那么就直接返回
        if (!tmp || timer->expire < tmp->expire) return;

        // 如果要调整的是链表的头节点的话，就将头节点摘下来，然后再插入进去
        if (timer == head) 
        {
            head = head->next;
            head->prev = NULL;
            timer->next = NULL;
            add_timer(timer, head);
        }
        else 
        {
            // 将timer摘出来，再插进去
            // ! 只能向后调整？
            timer->prev->next = timer->next;
            timer->next->prev = timer->prev;
            add_timer(timer, timer->next);
        }
    }

    void del_timer(util_timer *timer)
    {
        if (!timer) return;
        if (timer == head && timer == tail) 
        {
            delete timer;
            head = NULL;
            tail = NULL;
            return;
        }
        if (timer == head)
        {
            head = head->next;
            head->prev = NULL;
            delete timer;
            return;
        }
        if (timer == tail)
        {
            tail = tail->prev;
            tail->next = NULL;
            delete timer;
            return;
        }
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        delete timer;
        return;
    }


    // 运行每个时钟绑定的连接？，然后剔除
    void tick()
    {
        if (!head) return;

        LOG_INFO("%s", "timer tick");
        Log::get_instance()->flush();

        time_t cur = time(NULL);
        util_timer *tmp = head;
        while (tmp)
        {
            if (cur < tmp->expire) break;
            tmp->cb_func(tmp->user_data);
            head = tmp->next;
            if (head) head->prev = NULL;
            delete tmp;
            tmp = head;
        }
    }
private:
    void add_timer(util_timer *timer, util_timer *lst_head)
    {
        util_timer *prev = lst_head;
        util_timer *tmp = prev->next;
        while (tmp)
        {
            if (timer->expire < tmp->expire)
            {
                prev->next = timer;
                timer->next = tmp;
                tmp->prev = timer;
                timer->prev = prev;
                break;
            }
            prev = tmp;
            tmp = tmp->next;
        }
        if (!tmp)
        {
            prev->next = timer;
            timer->prev = prev;
            timer->next = NULL;
            tail = timer;
        }
    }

private:
    util_timer *head;
    util_timer *tail;
};


#endif // !LST_TIMER