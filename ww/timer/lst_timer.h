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

        // �����һ��Ϊ�գ�������һ������ʱ��С�������������ô��ֱ�ӷ���
        if (!tmp || timer->expire < tmp->expire) return;

        // ���Ҫ�������������ͷ�ڵ�Ļ����ͽ�ͷ�ڵ�ժ������Ȼ���ٲ����ȥ
        if (timer == head) 
        {
            head = head->next;
            head->prev = NULL;
            timer->next = NULL;
            add_timer(timer, head);
        }
        else 
        {
            // ��timerժ�������ٲ��ȥ
            // ! ֻ����������
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


    // ����ÿ��ʱ�Ӱ󶨵����ӣ���Ȼ���޳�
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