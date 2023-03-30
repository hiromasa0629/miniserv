#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/select.h>
#include <fcntl.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <errno.h>
#include <strings.h>

typedef struct s_mySocket 
{
	int					socketfd;
	struct sockaddr_in	addr;
	fd_set				default_readfds;	
}	t_mySocket;

typedef struct s_client
{
	int					fd;
	unsigned long long	id;
	struct s_client*	next;
}	t_client;

t_client*			clients = NULL;
t_mySocket			sock;
unsigned long long	g_id = 0;
int					maxfd = 0;

void	ft_putstr_fd(int fd, char* str)
{
	if (!str)
		return ;
	write(fd, str, strlen(str));
}

void	handle_fatal_error(void)
{
	t_client*	tmp;
	
	ft_putstr_fd(2, "Fatal error\n");
	if (maxfd == 0)
	{
		if (sock.socketfd == 0)
			exit(1);
		close(sock.socketfd);
		exit(1);
	}
	else
	{
		for (int i = 0; i < maxfd + 1; i++)
			if (FD_ISSET(i, &sock.default_readfds))
				close(i);
		while (clients)
		{
			tmp = clients->next;
			free(clients);
			clients = tmp;
		}
	}
}

void	init(char* port)
{
	sock.socketfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sock.socketfd == -1)
		handle_fatal_error();
	int tmp = 1;																// To be deleted
	setsockopt(sock.socketfd, SOL_SOCKET, SO_REUSEADDR, &tmp, sizeof(tmp));		// To be deleted
	bzero(&(sock.addr), sizeof(sock.addr));
	sock.addr.sin_family = AF_INET;
	sock.addr.sin_addr.s_addr = htonl(2130706433);
	sock.addr.sin_port = htons(atoi(port));
	if ((bind(sock.socketfd, (const struct sockaddr *)&(sock.addr), sizeof(sock.addr))) != 0)
		handle_fatal_error();
	if (listen(sock.socketfd, SOMAXCONN) != 0)
		handle_fatal_error();
}

void	ft_lstadd(int clientfd, unsigned long long id)
{
	t_client*	tmp;
	
	tmp = clients;
	while (tmp && tmp->next)
		tmp = tmp->next;
	
	t_client*	client;
	client = calloc(1, sizeof(t_client));
	if (!client)
		handle_fatal_error();
	client->id = id;
	client->fd = clientfd;
	client->next = NULL;
	if (!tmp)
		clients = client;
	else
		tmp->next = client;
}

void	ft_lstdelone(unsigned long long id)
{
	t_client*	tmp;
	t_client*	tmp_next;
	
	tmp = clients;
	if (tmp->id == id)
	{
		free(tmp);
		return ;
	}
	while (tmp)
	{
		tmp_next = tmp->next;
		if (tmp_next && tmp_next->id == id)
		{
			tmp->next = tmp_next->next;
			FD_CLR(tmp->fd, &sock.default_readfds);
			free(tmp_next);
			return ;
		}
		tmp = tmp_next;
	}
}

t_client*	ft_getlstbyfd(int fd)
{
	t_client*	tmp;
	
	tmp = clients;
	while (tmp)
	{
		if (tmp->fd == fd)
			return (tmp);
		tmp = tmp->next;
	}
	return (NULL);
}

void	send_all(int skip_fd, char* s)
{
	t_client*	tmp;
	
	tmp = clients;
	while (tmp)
	{
		if (tmp->fd != skip_fd)
		{
			if (send(tmp->fd, s, strlen(s), 0) == -1)
				handle_fatal_error();
		}
		tmp = tmp->next;
	}
}

void	accept_connection(void)
{
	int				clientfd;
	struct sockaddr	cli;
	socklen_t		cli_len;
	char			s[4096];
	
	cli_len = sizeof(cli);
	clientfd = accept(sock.socketfd, (struct sockaddr *)&cli, &cli_len);
	if (clientfd == -1)
		handle_fatal_error();
	ft_lstadd(clientfd, g_id++);
	if (clientfd > maxfd)
		maxfd = clientfd;
	FD_SET(clientfd, &sock.default_readfds);
	sprintf(s, "server: client %d just arrived\n", clientfd);
	send_all(clientfd, s);
}

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

void	handle_message(int fd, char* full_msg)
{
	char*	msg;
	int		ret;
	char*	response;
	
	msg = NULL;
	ret = extract_message(&full_msg, &msg);
	while (ret != 0)
	{
		if (ret == -1)
			handle_fatal_error();
		response = (char *)malloc(sizeof(char) * (strlen(msg) + 100));
		if (!response)
			handle_fatal_error();
		strcpy(response, msg);
		free(msg);
		msg = NULL;
		send_all(fd, response);
		free(response);
		response = NULL;
		ret = extract_message(&full_msg, &msg);
	}
}

void	handle_pollin(int fd)
{
	t_client*	client;
	char		buf[4096];
	ssize_t		size;
	char		s[4096];
	char*		full_msg;
	
	client = ft_getlstbyfd(fd);
	bzero(buf, 4096);
	size = recv(fd, buf, 4096, 0);
	if (size <= 0)
	{
		sprintf(s, "server: client %d just left\n", fd);
		send_all(client->fd, s);
		ft_lstdelone(client->id);
	}
	else
	{
		full_msg = NULL;
		while (size > 0)
		{
			if (!full_msg)
				full_msg = str_join(NULL, buf);
			else
				full_msg = str_join(full_msg, buf);
			bzero(buf, 4096);
			size = recv(fd, buf, 4096, 0);
		}
		handle_message(fd, full_msg);
	}
}

int	main(int ac, char** av)
{
	if (ac != 2)
	{
		ft_putstr_fd(2, "Wrong number of arguments\n");
		exit(1);
	}
	
	init(av[1]);

	int			select_ready;
	fd_set		readfds;
	t_client*	tmp;

	maxfd = sock.socketfd;
	FD_ZERO(&readfds);
	FD_ZERO(&(sock.default_readfds));
	FD_SET(sock.socketfd, &(sock.default_readfds));
	while (1)
	{
		readfds = sock.default_readfds;
		select_ready = select(maxfd + 1, &readfds, NULL, NULL, NULL);
		if (select_ready == -1)
			handle_fatal_error();
		if (select_ready == 0)
			continue ;
		if (FD_ISSET(sock.socketfd, &readfds))
		{		
			accept_connection();
			FD_CLR(sock.socketfd, &readfds);
		}
		else
		{
			tmp = clients;
			
			while (tmp)
			{
				if (FD_ISSET(tmp->fd, &readfds))
					handle_pollin(tmp->fd);
				tmp = tmp->next;
			}
		}
	}
}
