#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <strings.h> 



///////////////////////////////////////////////////////////////////////////////////////////////////
// constants
///////////////////////////////////////////////////////////////////////////////////////////////////
// port
#define DEFAULT_PORT 7777
#define MAX_PORT_NUMBER 49151
#define MIN_PORT_NUMBER 1024
// main socket
#define REQUESTS_QUEUE_SIZE 10
#define ERR_SOCKET_DESCRIPTOR 100
#define ERR_SOCKET_OPTION 110
#define ERR_SOCKET_BIND 120
#define ERR_SOCKET_LISTEN 130



///////////////////////////////////////////////////////////////////////////////////////////////////
// function declarations
///////////////////////////////////////////////////////////////////////////////////////////////////
/*
	Obtains the port number to be used for listening. The port is obtained from the command line
	arguments.
	Retruns port number if success, -1 no port specified or the port is invalid
*/
int obtain_port();
/*
	Prints a template for starting the server
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
	Deals with the error produced in init_socket.
*/
void process_init_socket_error(int err, int sd);
/*
	Used as main function for request threads. Processes request from the socket passed in
	arguments and distributes it to request specific functions. Locks the main thread until the 
	socket from argument is copied - signals on condition variable.
*/
void* manage_request(void* p_socket);


///////////////////////////////////////////////////////////////////////////////////////////////////
// global variables
///////////////////////////////////////////////////////////////////////////////////////////////////



///////////////////////////////////////////////////////////////////////////////////////////////////
// main
///////////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[]) 
{
	int port = obtain_port(argc, argv);
	if (port < 0)
	{
		print_usage();
		port = DEFAULT_PORT;
		printf("Default port %d assigned\n", DEFAULT_PORT);
	}
	else
		printf("Server started on port: %d\n", port);

	// initialize the main socket
	int server_socket = -1;
	int init_socket_res = init_socket(port, &server_socket);
	if (init_socket_res != 0)
	{
		process_init_socket_error(init_socket_res, server_socket);
		return -1;
	}

	return 0;
}



int obtain_port(int argc, char* argv[])
{
	int  option = 0;
	char port[256]= "";

	while ((option = getopt(argc, argv,"p:")) != -1) {
		switch (option) {
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

	return res >= MIN_PORT_NUMBER && res <+ MAX_PORT_NUMBER ? res : -1;
}



void print_usage() 
{
	    printf("Usage: server -p <port [1024 - 49151]> \n");
}



int init_socket(int port, int* p_socket)
{
    struct sockaddr_in server_addr;
    int ssd; // server socket descriptor, client socket descriptor
    
    // obtain server socket descriptor
    ssd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (ssd == -1)
		return ERR_SOCKET_DESCRIPTOR;

	*p_socket = ssd;

    // set server socket options
    int reuse_addr_val = 1;
    if (setsockopt(ssd, SOL_SOCKET, SO_REUSEADDR, (char*) &reuse_addr_val, sizeof(int)) == -1)
		return ERR_SOCKET_OPTION;

    // clear server addres
    bzero((char*) &server_addr, sizeof(server_addr));

    // initialize the server addres
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // bind the socket to the address
    if (bind(ssd, (struct sockaddr*) &server_addr, sizeof(server_addr)) == -1)
		return ERR_SOCKET_BIND;

    // start listening on the socket
    if (listen(ssd, REQUESTS_QUEUE_SIZE) == -1)
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



///////////////////////////////////////////////////////////////////////////////////////////////////
// manage_request
///////////////////////////////////////////////////////////////////////////////////////////////////

void* manage_request(void* p_socket)
{
	return NULL;
}
	
