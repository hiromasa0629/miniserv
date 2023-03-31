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
	int					id;
	struct s_client*	next;
}	t_client;

t_client*			clients = NULL;
t_mySocket			sock;
int					g_id = 0;
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
	if (sock.socketfd != 0)
		close(sock.socketfd);
	while (clients)
	{
		tmp = clients;
		clients = tmp->next;
		close(tmp->fd);
		free(tmp);
	}
}

void	init(char* port)
{
	sock.socketfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sock.socketfd == -1)
		handle_fatal_error();
	// int tmp = 1;																// To be deleted
	// setsockopt(sock.socketfd, SOL_SOCKET, SO_REUSEADDR, &tmp, sizeof(tmp));		// To be deleted
	bzero(&(sock.addr), sizeof(sock.addr));
	sock.addr.sin_family = AF_INET;
	sock.addr.sin_addr.s_addr = htonl(2130706433);
	sock.addr.sin_port = htons(atoi(port));
	if ((bind(sock.socketfd, (const struct sockaddr *)&(sock.addr), sizeof(sock.addr))) != 0)
		handle_fatal_error();
	if (listen(sock.socketfd, SOMAXCONN) != 0)
		handle_fatal_error();
}

void	add_client(int clientfd, int id)
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

void	remove_client(int fd)
{
	t_client*	tmp;
	t_client*	tmp_next;
	
	tmp = clients;
	if (tmp->fd == fd)
	{
		clients = tmp->next;
		FD_CLR(tmp->fd, &sock.default_readfds);
		close(tmp->fd);
		free(tmp);
		return ;
	}
	while (tmp)
	{
		tmp_next = tmp->next;
		if (tmp_next && tmp_next->fd == fd)
		{
			tmp->next = tmp_next->next;
			FD_CLR(tmp_next->fd, &sock.default_readfds);
			close(tmp_next->fd);
			free(tmp_next);
			return ;
		}
		tmp = tmp_next;
	}
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
	sprintf(s, "server: client %d just arrived\n", g_id);
	add_client(clientfd, g_id++);
	if (clientfd > maxfd)
		maxfd = clientfd;
	FD_SET(clientfd, &sock.default_readfds);
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
		handle_fatal_error();
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

void	handle_message(t_client* cli, char* full_msg)
{
	char*	response;
	char	front[4096];

	response = (char *)malloc(sizeof(char) * (strlen(full_msg) + 100));
	if (!response)
		handle_fatal_error();
	bzero(front, 4096);
	sprintf(front, "client %d: ", cli->id);
	response = str_join(NULL, front);
	response = str_join(response, full_msg);
	send_all(cli->fd, response);
	free(response);
}

void	handle_pollin(t_client* cli)
{
	char		buf[4096];
	ssize_t		size = 1;
	char		s[4096];
	char*		full_msg;
	
	bzero(buf, 4096);
	size = recv(cli->fd, buf, 1, 0);
	if (size == 0)
	{
		sprintf(s, "server: client %d just left\n", cli->id);
		send_all(cli->fd, s);
		remove_client(cli->fd);
	}
	else
	{
		full_msg = NULL;
		while (size == 1)
		{
			if (buf[0] == '\n')
			{
				full_msg = str_join(full_msg, buf);
				break ;
			}
			full_msg = str_join(full_msg, buf);
			size = recv(cli->fd, buf, 1, 0);
		}
		handle_message(cli, full_msg);
		free(full_msg);
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
			accept_connection();
		else
		{
			tmp = clients;
			while (tmp)
			{
				if (FD_ISSET(tmp->fd, &readfds))
					handle_pollin(tmp);
				tmp = tmp->next;
			}
		}
	}
}
