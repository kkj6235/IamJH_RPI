#include <stdlib.h>

typedef struct s_list
{
	void *bullet;
	struct s_list *next;
}t_list;

t_list	*lstlast(t_list *lst);
void	lstadd_back(t_list **lst, t_list *new);
void	lstadd_front(t_list **lst, t_list *new);
void	lstclear(t_list **lst);
void	lstdelone(t_list *lst);
void	lstiter(t_list *lst, void (*f)(void *));
t_list	*lstnew(void *bullet);
