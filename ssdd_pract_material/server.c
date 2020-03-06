#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

void print_usage() {
	    printf("Usage: server -p puerto \n");
}

int main(int argc, char *argv[]) {
	int  option = 0;
	char port[256]= "";

	while ((option = getopt(argc, argv,"p:")) != -1) {
		switch (option) {
		    	case 'p' : 
				strcpy(port, optarg);
		    		break;
		    	default: 
				print_usage(); 
		    		exit(-1);
		    }
	}
	if (strcmp(port,"")==0){
		print_usage(); 
		exit(-1);
	}


	printf("Port: %s\n", port);


	//  INSERT SERVER CODE HERE
	
	return 0;
}
	
