#include "lst.h"

t_list  *lstlast(t_list *lst)
{
        if (lst == 0)
                return (0);
        while (lst->next)
                lst = lst->next;
        return (lst);
}

void    lstadd_back(t_list **lst, t_list *new)
{
        if (lst == 0 || new == 0)
                return ;
        if ((*lst) == 0)
                *lst = new;
        else
                lstlast(*lst)->next = new;
}

void    lstadd_front(t_list **lst, t_list *new)
{
        if (lst == 0 || new == 0)
                return ;
        new->next = *lst;
        *lst = new;
}

void    lstclear(t_list **lst)
{
        t_list  *tmp;

        if (lst == 0)
                return ;
        while (*lst)
        {
			tmp = *lst;
            *lst = (*lst)->next;
            lstdelone(tmp);
        }
}

void    lstdelone(t_list *lst)
{
    free(lst->bullet);
    free(lst);
}

void    lstiter(t_list *lst, void (*f)(void *))
{
        if (lst == 0 || f == 0)
                return ;
        while (lst)
        {
                f(lst->bullet);
                lst = lst->next;
        }
}

t_list  *lstnew(void *bullet)
{
        t_list  *res;

        res = (t_list *)malloc(sizeof(t_list));
        if (res == 0)
                return (0);
        res->next = 0;
        res->bullet = bullet;
        return (res);
}

