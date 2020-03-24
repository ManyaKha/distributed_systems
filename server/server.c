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
#include <signal.h>
#include <errno.h>
#include "user_dao.h"



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
#define REQ_LIST_USERS "LIST_USERS"
#define REQ_LIST_CONTENT "LIST_CONTENT"
// register
#define MAX_USERNAME_LEN 256
#define REGISTER_SUCCESS 0
#define REGISTER_NON_UNIQUE_USERNAME 1
#define REGISTER_OTHER_ERROR 2
// unregister
#define UNREGISTER_SUCCESS 0
#define UNREGISTER_NO_SUCH_USER 1
#define UNREGISTER_OTHER_ERROR 2
// publish
#define MAX_FILENAME_LEN 256
#define MAX_NUMBER_OF_FILES 100000
// list users
#define LIST_USERS_SUCCESS 0
#define LIST_USERS_NO_SUCH_USER 1
#define LIST_USERS_DISCONNECTED 2
#define LIST_USERS_OTHER_ERROR 3
// send users list
#define SEND_USERS_LIST_SUCCESS 0
#define SEND_USERS_LIST_ERR_NUM_OF_USERS 1
#define SEND_USERS_LIST_ERR_USERNAME 2
#define SEND_USERS_LIST_ERR_IP 3
#define SEND_USERS_LIST_ERR_PORT 4
// list content
#define LIST_CONTENT_SUCCESS 0
#define LIST_CONTENT_NOT_REGISTERED 1
#define LIST_CONTENT_DISCONNECTED 2
#define LIST_CONTENT_NO_SUCH_FILES_OWNER 3
#define LIST_CONTENT_OTHER_ERROR 4
// send content list
#define SEND_CONTENT_LIST_SUCCESS 0
#define SEND_CONTENT_LIST_ERR_NUM_OF_FILES 1
#define SEND_CONTENT_LIST_ERR_FILENAME 2


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
	starts listening for ctrl+c to finish the program.
	Returns 1 on success and 0 on fail
*/
int start_listening_sigint();
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
	Reads username from the socket and puts it's value into the address space pointed by username.
	Returns number of characters read
*/
int read_username(int socket, char* username);

/*
	checks if username is valid.
	Returns 1 if yes 0 if no
*/
int is_username_valid(char* username);

void unregister(int socket);
/*
	checks if the user with the username is connected to the server.
	Returns 1 if the user is connected and 0 if no
*/
int is_connected(char* username);

void list_users(int socket);
/*
	dynamically allocates an array of users which are connected to the system, so
	it has to be deleted afterwards.
	Returns number of connected users
*/
uint32_t get_connected_users_list(user** p_users_list);

/*
	Sends list of users through the socket. First username is send, then ip and finally port.
	Returns:
	SEND_USERS_LIST_SUCCESS 			- success
	SEND_USERS_LIST_ERR_NUM_OF_USERS 	- could not send number of users
	SEND_USERS_LIST_ERR_USERNAME 		- could not send username
	SEND_USERS_LIST_ERR_IP 				- could not send ip
	SEND_USERS_LIST_ERR_PORT 			- could not send port
*/
int send_users_list(int socket, user* users_list, uint32_t num_of_users);

void list_content(int socket);
/*
	Sends list of content (names of files) through the socket.
	Returns:
	SEND_CONTENT_LIST_SUCCESS 			- success
	SEND_CONTENT_LIST_ERR_NUM_OF_FILES 	- could not send number of files
	SEND_CONTENT_LIST_ERR_FILENAME 		- could not send filename
*/
int send_content_list(int socket, char** content_list, uint32_t num_of_files);


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
/*
	flat to mark if the programm should continue. if 1 then continue, if 0 then the main loop
	should stop. It is set to 1 after pressing ctrl + c
*/
int is_running = 1;


///////////////////////////////////////////////////////////////////////////////////////////////////
// main
///////////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[]) 
{
	int port = obtain_port(argc, argv);
	port = process_obtain_port_result(port);

	// TODO obtain local ip address
	char addr[] = "192.168.0.102";	// just temporary, needs to be changed

	printf("init server %s:%d\n", addr, port);

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

	// init storage
	int init_user_dao_res = init_user_dao();
	if (init_user_dao_res != INIT_USER_DAO_SUCCESS)
	{
		printf("ERROR main - could not initialize user dao. Code: %d\n", init_user_dao_res);
		return -1;
	}
	
	// start waiting for requests
	struct sockaddr_in client_addr;
	int client_socket;
    socklen_t clinet_addr_size = sizeof(struct sockaddr_in);

	// start detecting ctrl + c
	if (!start_listening_sigint())
		return -1;

    while (is_running)
    {
        // accept connection from a client
        client_socket = accept(server_socket, (struct sockaddr*) &client_addr, &clinet_addr_size);

		if (client_socket >= 0)
		{
			if (pthread_create(&t_request, &attr_req_thread, manage_request, (void*) &client_socket) != 0)
				perror("ERROR main - could not create request thread");

			if (wait_till_socket_copying_is_done() != 0)
				return -1;
		}
		else if (errno != EINTR) // if EINTR then ctrl+c was pressed, finish
		{
			perror("ERROR main - could not accept request from socket");
			return -1;
		}
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



void set_exit_flat(int val)
{
	is_running = 0;
}



int start_listening_sigint()
{
	struct sigaction act;
	memset(&act, '\0', sizeof(act));

	act.sa_handler = &set_exit_flat;

	if (sigaction(SIGINT, &act, NULL) != 0)
	{
		perror("ERROR start_listening_siginit - could not perform sigaction");
		return 0;
	}

	return 1;
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

	int destroy_user_dao_res = destroy_user_dao();
	if (destroy_user_dao_res != DESTROY_USER_DAO_SUCCESS)
	{
		printf("ERROR clean up - could not destroy user dao. Code: %d\n", destroy_user_dao_res);
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

	pthread_exit(NULL);
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
	else if (strcmp(req_type, REQ_LIST_USERS) == 0)
		list_users(socket);
	else if (strcmp(req_type, REQ_LIST_CONTENT) == 0)
		list_content(socket);
	else
		printf("ERROR identify_and_process_request - no such request type\n");
}



///////////////////////////////////////////////////////////////////////////////////////////////////
// register
///////////////////////////////////////////////////////////////////////////////////////////////////

void register_user(int socket)
{
	uint8_t result = REGISTER_SUCCESS;
	
	char username[MAX_USERNAME_LEN + 1];
	if (read_username(socket, username) > 0)	// username specified
	{
		if (is_username_valid(username))
		{
			int create_user_res = create_user(username);

			switch (create_user_res)
			{
				case CREATE_USER_SUCCESS 	: result = REGISTER_SUCCESS; break;
				case CREATE_USER_ERR_EXISTS : result = REGISTER_NON_UNIQUE_USERNAME; break;
				default : 
					result = REGISTER_OTHER_ERROR;
					perror("ERROR register user other error");
			}
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
	response[0] = result;
	response[1] = '\0';
	
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



///////////////////////////////////////////////////////////////////////////////////////////////////
// unregister
///////////////////////////////////////////////////////////////////////////////////////////////////

void unregister(int socket)
{
	uint8_t res = UNREGISTER_SUCCESS;

	char username[MAX_USERNAME_LEN + 1];
	if (read_username(socket, username) > 0)	// username specified
	{
		int delete_res = delete_user(username);

		switch (delete_res)
		{
			case DELETE_USER_SUCCESS 		: res = UNREGISTER_SUCCESS; break;
			case DELETE_USER_ERR_NOT_EXISTS : res = UNREGISTER_NO_SUCH_USER; break;
			default							: res = UNREGISTER_OTHER_ERROR; 
		}
	}
	else
	{
		printf("ERROR unregister - no username specified\n");
		res = UNREGISTER_OTHER_ERROR;
	}
	
	char response[2];
	response[0] = res;
	response[1] = '\0';
	
	if (send_msg(socket, response, 2) != 0)
		printf("ERROR unregister - could not send response\n");
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
	uint8_t res = LIST_USERS_SUCCESS;
	user* users_list = NULL;
	uint32_t num_of_users = 0;

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
	response_res_code[0] = res;
	response_res_code[1] = '\0';

	if (send_msg(socket, response_res_code, 2) == 0)
	{
		if (res == LIST_USERS_SUCCESS)
		{
			int send_res = send_users_list(socket, users_list, num_of_users);

			if (send_res != SEND_USERS_LIST_SUCCESS)
				printf("ERROR list_users - could not send users. Code: %d\n", send_res);
		}
	}
	else
		printf("ERROR list_users - could not send response\n");

	if (users_list != NULL)
		free(users_list);
}



uint32_t get_connected_users_list(user** p_users_list)
{
	// TODO do real implementation
	printf("NOT YET IMPLEMENTED get_connected_users_list\n");
	
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



int send_users_list(int socket, user* users_list, uint32_t num_of_users)
{
	// send number of users
	char str_num_of_users[8]; // max number of users is 4 000 000
	sprintf(str_num_of_users, "%d", num_of_users);
	if (send_msg(socket, str_num_of_users, strlen(str_num_of_users) + 1) != 0)
		return SEND_USERS_LIST_ERR_NUM_OF_USERS;

	// send users' data
	for (uint32_t i = 0; i < num_of_users; i++)
	{
		if (send_msg(socket, users_list[i].username, strlen(users_list[i].username) + 1) != 0)
			return SEND_USERS_LIST_ERR_USERNAME;
		if (send_msg(socket, users_list[i].ip, strlen(users_list[i].ip) + 1) != 0)
			return SEND_USERS_LIST_ERR_IP;
		if (send_msg(socket, users_list[i].port, strlen(users_list[i].port) + 1) != 0)
			return SEND_USERS_LIST_ERR_PORT;
	}

	return SEND_USERS_LIST_SUCCESS;
}



///////////////////////////////////////////////////////////////////////////////////////////////////
// list_content
///////////////////////////////////////////////////////////////////////////////////////////////////

void list_content(int socket)
{
	uint8_t res = LIST_CONTENT_SUCCESS;
	char** content_list = NULL;
	uint32_t num_of_files = 0;

	char username[MAX_USERNAME_LEN + 1];
	if (read_username(socket, username) > 0) // if requesting user specified
	{
		if (is_registered(username))
		{
			if (is_connected(username))
			{
				char content_owner[MAX_USERNAME_LEN + 1];
				if (read_username(socket, content_owner) > 0) // content owner specified
				{
					int get_f_res = get_user_files_list(content_owner, &content_list, 
						&num_of_files);

					if (get_f_res == GET_USER_FILES_LIST_ERR_NO_SUCH_USER)
						res = LIST_CONTENT_NO_SUCH_FILES_OWNER;
					else if (get_f_res != GET_USER_FILES_LIST_SUCCESS)
						res = LIST_CONTENT_OTHER_ERROR;
				}
				else // no content owner specified
					res = LIST_CONTENT_NO_SUCH_FILES_OWNER;
			}
			else
				res = LIST_CONTENT_DISCONNECTED;
		}
		else // requesting user not registered
			res = LIST_CONTENT_NOT_REGISTERED;
	}
	else // no requesting user's username specified
	{
		printf("ERROR list_content - no requesting user specified");
		res = LIST_CONTENT_OTHER_ERROR;
	}

	// send result
	char response_res_code[2];
	response_res_code[0] = res;
	response_res_code[1] = '\0';

	if (send_msg(socket, response_res_code, 2) == 0)
	{
		if (res == LIST_CONTENT_SUCCESS)
		{
			int send_res = send_content_list(socket, content_list, num_of_files);

			if (send_res != SEND_CONTENT_LIST_SUCCESS)
				printf("ERROR list_content - could not send content. Code: %d\n", send_res);
		}
	}
	else
		printf("ERROR list_content - could not send response\n");

	if (content_list != NULL)
	{
		for (uint32_t i = 0; i < num_of_files; i++)
			free(content_list[i]);
		free(content_list);
	}
}



int send_content_list(int socket, char** content_list, uint32_t num_of_files)
{
	// send number of files
	char str_num_of_files[7];	// max number of files is 100000
	sprintf(str_num_of_files, "%d", num_of_files);

	if (send_msg(socket, str_num_of_files, strlen(str_num_of_files) + 1) != 0)
		return SEND_CONTENT_LIST_ERR_NUM_OF_FILES;

	// send users' data
	for (uint32_t i = 0; i < num_of_files; i++)
	{
		if (send_msg(socket, content_list[i], strlen(content_list[i]) + 1) != 0)
			return SEND_CONTENT_LIST_ERR_FILENAME;
	}

	return SEND_CONTENT_LIST_SUCCESS;
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
	
