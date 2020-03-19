#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <strings.h> 
#include <pthread.h>
#include "lines.h"
#include <regex.h>



///////////////////////////////////////////////////////////////////////////////////////////////////
// constants
///////////////////////////////////////////////////////////////////////////////////////////////////
// port
#define DEFAULT_PORT 7777
#define MAX_PORT_NUMBER 49151
#define MIN_PORT_NUMBER 1024
// ip address
#define MAX_IP_ADDR_LEN 15
// main socket
#define REQUESTS_QUEUE_SIZE 10
#define ERR_SOCKET_DESCRIPTOR 100
#define ERR_SOCKET_OPTION 110
#define ERR_SOCKET_BIND 120
#define ERR_SOCKET_LISTEN 130
// request
#define MAX_REQ_TYPE_LEN 20
#define REQ_REGISTER "REGISTER"
#define REQ_UNREGISTER "UNREGISTER"
// register
#define MAX_USERNAME_LEN 256
#define REGISTER_SUCCESS 0
#define REGISTER_NON_UNIQUE_USERNAME 1
#define REGISTER_OTHER_ERROR 2
// store user
#define STORE_USER_SUCCESS 0
// unregister
#define UNREGISTER_SUCCESS 0
#define UNREGISTER_NO_SUCH_USER 1
#define UNREGISTER_OTHER_ERROR 2
// delete user
#define DELETE_USER_SUCCESS 0
#define DELETE_USER_NO_SUCH_USER 1
// list users
#define LIST_USERS_SUCCESS 0
#define LIST_USERS_NO_SUCH_USER 1
#define LIST_USERS_DISCONNECTED 2
#define LIST_USERS_OTHER_ERROR 3



///////////////////////////////////////////////////////////////////////////////////////////////////
// structs
///////////////////////////////////////////////////////////////////////////////////////////////////

struct user_data {
	char username[MAX_USERNAME_LEN + 1];
	char ip[MAX_IP_ADDR_LEN + 1];
	char port[6];
};

typedef struct user_data user;



///////////////////////////////////////////////////////////////////////////////////////////////////
// function declarations
///////////////////////////////////////////////////////////////////////////////////////////////////
/*
	Obtains the port number to be used for listening. The port is obtained from the command line
	arguments.
	Returns port number if success, -1 if no port specified or the port is invalid
*/
int obtain_port();
/*
	processes result from the obtain_port function and prints approporiate message.
	Returns obtained_port or default port if errors occured.
*/
int process_obtain_port_result(int port);
/*
	Prints a cmd template for starting the server
*/
void print_usage();
/*
	Creates a socket to listen on the specified port and assigns the socket descriptor to p_socket.
	Returns:
	ERR_SOCKET_DESCRIPTOR	- could not create the socket
	ERR_SOCKET_OPTION 		- could not set options for the socket
	ERR_SOCKET_BIND 		- could not bind the socket
	ERR_SOCKET_LISTEN 		- could not start listening
*/
int init_socket(int port, int* p_socket);
/*
	initializes mutex_csd and cond_csd.
	Returns 0 on success and -1 on fail.
*/
int init_copy_client_socket_concurrency_mechanisms();
/*
	initializes attributes for request thread.
	Returns 0 on success and -1 on fail
*/
int init_request_thread_attr(pthread_attr_t* attr);
/*
	Deals with the error produced in init_socket.
*/
void process_init_socket_error(int err, int sd);
/*
	Waits in main thread until client socket descriptor is copied to new thread
*/
int wait_till_socket_copying_is_done();
/*
	cleans up after main function.
	Returns 0 on success and -1 on fail.
*/
int clean_up(int server_socket, pthread_attr_t* p_attr);
/*
	Once a request arrives to the server through the general socket (the socket bound to the
	port specified in cmd) the server will create a new thread for processing the request and
	this is the function which will be runnig in the newly created thread. The function will lock 
	mutex_csd while copying the client socket to a local variable and when it is finish it will
	signal on cond_csd.
*/
void* manage_request(void* p_socket);
/*
	Reads from the socket in order to identify request type, i.e. register, unregister, connect....
	If the request could be identified then a request specific function is called. If the request
	could not be identified an approporiate message will be send back to the socket. 
*/
void identify_and_process_request(int socket);

void register_user(int socket);

/*
	Returns number of characters read
*/
int read_username(int socket, char* username);

/*
	checks if username is valid.
	Returns 1 if yes 0 if no
*/
int is_username_valid(char* username);
/*
	checks if the user with the username is registered.
	Returns 1 if the user is registered and 0 if no
*/
int is_registered(char* username);

int store_user(char* username);

void unregister(int socket);

int delete_user(char* username);
/*
	checks if the user with the username is connected to the server.
	Returns 1 if the user is connected and 0 if no
*/
int is_connected(char* username);

void list_users(int socket);

int get_connected_users_list(user** users_list);

void send_users_list(int socket, user* users_list, int num_of_users);


///////////////////////////////////////////////////////////////////////////////////////////////////
// global variables
///////////////////////////////////////////////////////////////////////////////////////////////////
/*
	mutex for preventing race condition during copying client socket description to a new thread.
*/
pthread_mutex_t mutex_csd;
/*
	condition for waiting until the cliend socket descriptor is copied to a new thread
*/
pthread_cond_t cond_csd;
/*
	used to check whether main thread should still wait for request thread (client socket not
	yet copied)
*/
int is_copied;


///////////////////////////////////////////////////////////////////////////////////////////////////
// main
///////////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[]) 
{
	int port = obtain_port(argc, argv);
	port = process_obtain_port_result(port);

	// initialize the main socket
	int server_socket = -1;
	int init_socket_res = init_socket(port, &server_socket);
	if (init_socket_res != 0)
	{
		process_init_socket_error(init_socket_res, server_socket);
		return -1;
	}

	if (init_copy_client_socket_concurrency_mechanisms() != 0)
		return -1;

	// init request thread
	pthread_attr_t attr_req_thread;
	if (init_request_thread_attr(&attr_req_thread) != 0)
		return -1;

	pthread_t t_request;
	
	// start waiting for requests
	struct sockaddr_in client_addr;
	int client_socket;
    socklen_t clinet_addr_size = sizeof(struct sockaddr_in);

    while (1)
    {
        // accept connection from a client
        client_socket = accept(server_socket, (struct sockaddr*) &client_addr, &clinet_addr_size);

		if (pthread_create(&t_request, &attr_req_thread, manage_request, (void*) &client_socket) != 0)
			perror("ERROR main - could not create request thread");

		if (wait_till_socket_copying_is_done() != 0)
			return -1;
    }

	return clean_up(server_socket, &attr_req_thread);
}



int obtain_port(int argc, char* argv[])
{
	int  option = 0;
	char port[256]= "";

	while ((option = getopt(argc, argv,"p:")) != -1) 
	{
		switch (option) 
		{
			case 'p' : 
				strcpy(port, optarg);
				break;
			default: 
				return -1;
		    }
	}
	if (strcmp(port,"")==0){
		return -1;
	}

	int res = -1;

	// cast to int
	sscanf(port, "%d", &res);

	return res >= MIN_PORT_NUMBER && res <= MAX_PORT_NUMBER ? res : -1;
}



int process_obtain_port_result(int port)
{
	int final_port = port;
	if (port < 0)
	{
		print_usage();
		final_port = DEFAULT_PORT;
		printf("Default port %d assigned\n", DEFAULT_PORT);
	}
	else
		printf("Server started on port: %d\n", port);

	return final_port;
}



void print_usage() 
{
	printf("Usage: server -p <port [1024 - 49151]> \n");
}



int init_socket(int port, int* p_socket)
{
    struct sockaddr_in server_addr;
    int sd; // server socket descriptor, client socket descriptor
    
    // obtain server socket descriptor
    sd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sd == -1)
		return ERR_SOCKET_DESCRIPTOR;

	*p_socket = sd;

    // set server socket options
    int reuse_addr_val = 1;
    if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char*) &reuse_addr_val, sizeof(int)) == -1)
		return ERR_SOCKET_OPTION;

    // clear server addres
    bzero((char*) &server_addr, sizeof(server_addr));

    // initialize the server addres
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // bind the socket to the address
    if (bind(sd, (struct sockaddr*) &server_addr, sizeof(server_addr)) == -1)
		return ERR_SOCKET_BIND;

    // start listening on the socket
    if (listen(sd, REQUESTS_QUEUE_SIZE) == -1)
		return ERR_SOCKET_LISTEN;

	return 0;
}



void process_init_socket_error(int err, int sd)
{
	if (err == ERR_SOCKET_DESCRIPTOR)
		perror("ERROR init_socket - could not obtain socket descriptor");
	else
	{
		switch (err)
		{
			case ERR_SOCKET_OPTION : 
				perror("ERROR init_socket - could not set options"); break;
			case ERR_SOCKET_BIND : 
				perror("ERROR init_socket - could not bind"); break;
			case ERR_SOCKET_LISTEN :
				perror("ERROR init_socket - could not start listening"); break;

			if (close(sd) == -1)
				perror ("ERROR process_init_socket_error - could not close the socket");
		}
	}
}



int init_copy_client_socket_concurrency_mechanisms()
{
	if (pthread_mutex_init(&mutex_csd, NULL) != 0)
	{
		perror("ERROR main - could not init mutex_csd");
		return -1;
	}

	if (pthread_cond_init(&cond_csd, NULL) != 0)
	{
		perror("ERROR main - could not init cond_csd");
		return -1;
	}

	return 0;
}



int init_request_thread_attr(pthread_attr_t* attr)
{
	if (pthread_attr_init(attr) != 0)
	{
		printf("ERROR init_request_thread_attr - could not init\n");
		return -1;
	}

    if (pthread_attr_setdetachstate(attr, PTHREAD_CREATE_DETACHED) != 0)
	{
		printf("ERROR init_request_thread_attr - could not set detach state\n");
		return -1;
	}

	return 0;
}



int wait_till_socket_copying_is_done()
{
	if (pthread_mutex_lock(&mutex_csd) != 0)
	{
		printf("ERROR wait_till_socket_copying_is_done - could not lock mutex_csd\n");
		return -1;
	}

	while (is_copied == 0)
	{
		if (pthread_cond_wait(&cond_csd, &mutex_csd) != 0)
		{
			printf("ERROR wait_till_socket_copying_is_done - during condition wait\n");
			return -1;	// if fails in this program, it's a serious error, stop the server
		}
	}

	is_copied = 0;

	if (pthread_mutex_unlock(&mutex_csd) != 0)
	{
		printf("ERROR wait_till_socket_copying_is_done - could not unlock mutex_csd\n");
		return -1;
	}

	return 0;
}



int clean_up(int server_socket, pthread_attr_t* p_attr)
{
	if (close(server_socket) != 0)
	{
		perror("ERROR clean up - could not close server_socket");
		return -1;
	}

	if (pthread_mutex_destroy(&mutex_csd) != 0)
	{
		perror("ERROR clean up - could not destroy mutex_csd");
		return -1;
	}

	if (pthread_cond_destroy(&cond_csd) != 0)
	{
		perror("ERROR clean up - could not destroy cond_csd");
		return -1;
	}

	if (pthread_attr_destroy(p_attr) != 0)
	{
		printf("ERROR clean up - could not destroy attributes\n");
		return -1;
	}

	return 0;
}



///////////////////////////////////////////////////////////////////////////////////////////////////
// manage_request
///////////////////////////////////////////////////////////////////////////////////////////////////

void* manage_request(void* p_socket)
{
	int socket = -1;

	// lock the main thread until the socket is copied
	if (pthread_mutex_lock(&mutex_csd) != 0)
	{
		printf("ERROR manage_request - could not lock mutex\n");
		return NULL;
	}

	memcpy(&socket, p_socket, sizeof(int));
	is_copied = 1;
	
	// notify that socket was copied
	if (pthread_cond_signal(&cond_csd) != 0)
		printf("ERROR manage_request - could not signal condition\n");

	if (pthread_mutex_unlock(&mutex_csd) != 0)
		printf("ERROR manage_request - could not unlock mutex\n");

	// process the request
	if (socket > 0)
		identify_and_process_request(socket);

	// close the client socket
	if (close(socket) != 0)
		perror("ERROR manage request - could not close client socket");

	return NULL;
}



void identify_and_process_request(int socket)
{
	char req_type[MAX_REQ_TYPE_LEN + 1];
	read_line(socket, req_type, MAX_REQ_TYPE_LEN);
	req_type[MAX_REQ_TYPE_LEN] = '\0'; // just in case if the request type is in wrong format

	// process request type
	if (strcmp(req_type, REQ_REGISTER) == 0)
		register_user(socket);
	else if (strcmp(req_type, REQ_UNREGISTER) == 0)
		unregister(socket);
}



///////////////////////////////////////////////////////////////////////////////////////////////////
// register
///////////////////////////////////////////////////////////////////////////////////////////////////

void register_user(int socket)
{
	int result = REGISTER_SUCCESS;
	
	char username[MAX_USERNAME_LEN + 1];
	if (read_username(socket, username) > 0)	// username specified
	{
		if (is_username_valid(username))
		{
			if (!is_registered(username))
			{
				if (store_user(username) != STORE_USER_SUCCESS)
					result = REGISTER_OTHER_ERROR;
			}
			else
				result = REGISTER_NON_UNIQUE_USERNAME;
		}
		else
			result = REGISTER_OTHER_ERROR;
	}
	else	// no username
	{
		printf("ERROR register_user - no username\n");
		result = REGISTER_OTHER_ERROR;
	}

	char response[2];
	sprintf(response, "%d", result);
	
	if (send_msg(socket, response, 2) != 0)
		printf("ERROR register_user - could not send message\n");
}



int is_username_valid(char* username)
{
	int res = 1;

	regex_t regex;
	if (regcomp(&regex, "^[A-Za-z0-9_]+$", REG_EXTENDED | REG_NOSUB) != 0)
	{
		printf("ERROR is_username_valid - wrong regex\n");
		return 0;
	}

	if (regexec(&regex, username, 0, NULL, 0) != 0)	// no match
		res = 0;

	regfree(&regex);

	return res;
}



int store_user(char* username)
{
	// TODO implement
	printf("NOT YET IMPLEMENTED store_user\n");

	return STORE_USER_SUCCESS;
}



///////////////////////////////////////////////////////////////////////////////////////////////////
// is_registered
///////////////////////////////////////////////////////////////////////////////////////////////////

int is_registered(char* username)
{
	// TODO IMPLEMENT
	printf("NOT YET IMPLEMENTED is_username_unique\n");
	return 1;
}



///////////////////////////////////////////////////////////////////////////////////////////////////
// unregister
///////////////////////////////////////////////////////////////////////////////////////////////////

void unregister(int socket)
{
	int res = UNREGISTER_SUCCESS;

	char username[MAX_USERNAME_LEN + 1];
	if (read_username(socket, username) > 0)	// username specified
	{
		int delete_res = delete_user(username);

		switch (delete_res)
		{
			case DELETE_USER_SUCCESS 		: res = UNREGISTER_SUCCESS; break;
			case DELETE_USER_NO_SUCH_USER 	: res = UNREGISTER_NO_SUCH_USER; break;
			default							: res = UNREGISTER_OTHER_ERROR; 
		}
	}
	else
	{
		printf("ERROR unregister - no username specified\n");
		res = UNREGISTER_OTHER_ERROR;
	}
	

	char response[2];
	sprintf(response, "%d", res);
	send_msg(socket, response, 2);
}



int delete_user(char* username)
{
	// TODO IMPLEMENT
	printf("NOT YET IMPLEMENTED delete_user\n");
	return DELETE_USER_SUCCESS;
}



//////////////////////////////////////////////////////////////////////////////////////////////////
// is_connected
///////////////////////////////////////////////////////////////////////////////////////////////////

int is_connected(char* username)
{
	// TODO IMPLEMENT
	printf("NOT YET IMPLEMENTED is_connected\n");

	return 1;
}



///////////////////////////////////////////////////////////////////////////////////////////////////
// list_users
///////////////////////////////////////////////////////////////////////////////////////////////////

void list_users(int socket)
{
	int res = LIST_USERS_SUCCESS;
	user* users_list = NULL;
	int num_of_users = -1;

	char username[MAX_USERNAME_LEN + 1];
	if (read_username(socket, username) > 0) // if user specified
	{
		if (is_registered(username))
		{
			if (is_connected(username))
				num_of_users = get_connected_users_list(&users_list);
			else
				res = LIST_USERS_DISCONNECTED;
		}
		else
			res = LIST_USERS_NO_SUCH_USER;
	}
	else // no username specified
	{
		printf("ERROR list_users - no user specified");
		res = LIST_USERS_OTHER_ERROR;
	}

	// send result
	char response_res_code[2];
	sprintf(response_res_code, "%d", res);
	send_msg(socket, response_res_code, 2);

	if (res == LIST_USERS_SUCCESS)
		send_users_list(socket, users_list, num_of_users);
}



int get_connected_users_list(user** p_users_list)
{
	// TODO do real implementation
	user* users = malloc(3 * sizeof(user));

	strcpy(users[0].username, "user1");
	strcpy(users[0].ip, "87.43.21.1");
	strcpy(users[0].port, "59000");

	strcpy(users[1].username, "user2");
	strcpy(users[1].ip, "87.43.21.2");
	strcpy(users[1].port, "59001");

	strcpy(users[2].username, "user3");
	strcpy(users[2].ip, "87.43.21.3");
	strcpy(users[2].port, "59003");

	*p_users_list = users;

	return 3;
}



void send_users_list(int socket, user* users_list, int num_of_users)
{
	// send number of users


}



///////////////////////////////////////////////////////////////////////////////////////////////////
// read_user_name
///////////////////////////////////////////////////////////////////////////////////////////////////

int read_username(int socket, char* username)
{
	int total_read = read_line(socket, username, MAX_USERNAME_LEN);
	username[MAX_USERNAME_LEN] = '\0'; // just in case if the username was not finished properly
	
	return total_read;
}
	
