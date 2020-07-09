/*	 Author: Brian Hambleton (hambletb)
 *	   Date: 12 August 2019
 *	Program: ftserver (ftserver.c)
 *  Description: Implements a server using sockets API that accepts a single connection at a time
 *  		 to communicate with a client, and connects to the client via a data connection.
 *		
 *		 Receives command (-l or -g) from client and:
 *		 	'-l': sends directory contents through data connection
 *		 	'-g': sends file contents through data connection
 *
 *  		 Usage: ./ftserver <SERVER_PORT> 
 */	

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>

#ifndef _FTSERVER_C
#define _FTSERVER_C

/* FUNCTION DECLARATIONS */
int setup_listen_socket(int*, char**);
int print_connection_info(char**, struct sockaddr_in);
int parse_command_input(char***, char*);
int allocate_commands(char***);
void deallocate_commands(char***);
int check_commands(char***, int);
int setup_data_socket(char*, char*);
int get_file_command(char*, int, int);
int list_command(int);
int send_file(char*, int);
void clear_command_inputs(char***);
/* END DECLARATIONS */


int main(int argc, char* argv[]) {
	int listenSocketFD, controlFD, dataFD;
	socklen_t sizeOfClientInfo;
        struct sockaddr_in clientAddress;
	char read_buffer[256];
	int num_bytes = -6;
	char* client_host_name = NULL;
	int get_host_info_return = 2;
	char** command_inputs;
	int i;
	
	if(argc != 2){
		fprintf(stdout, "Incorrect Arguments.\nUsage: %s <server port>\n", argv[0]);
		exit(1);
	}

	sizeOfClientInfo = sizeof(clientAddress);
	setup_listen_socket(&listenSocketFD, argv);
	//allocate command_inputs 2D array
	allocate_commands(&command_inputs);
	
	while(1){
		fprintf(stdout, "Server listening for connections...\n");
		controlFD = accept(listenSocketFD, (struct sockaddr *)&clientAddress, &sizeOfClientInfo);

		//get connected client's host info & print
		get_host_info_return = print_connection_info(&client_host_name, clientAddress);
		
		clear_command_inputs(&command_inputs);
		
		//get command from client
		memset(&read_buffer, '\0', sizeof(read_buffer));
		recv(controlFD, read_buffer, 255, 0);	
		//fprintf(stdout, "SERVER: Received %s\n", read_buffer);
		
		//parse read_buffer into 
		parse_command_input((&command_inputs), read_buffer);

		
		//fprintf(stdout, "\tDATA PORT= %s\n\tCOMMAND= %s\n\tFILENAME= (%s)\n", command_inputs[0], 
		//	command_inputs[1], command_inputs[2]);
		if((check_commands(&command_inputs, controlFD)) == 1){ 
			continue; // skip data connection setup, do cleanup & exit
		}
		else{
			//connect with client_host_name & atoi(command_inputs[0])
			dataFD = setup_data_socket(client_host_name, command_inputs[0]);
			if((strcmp(command_inputs[1], "-l")) == 0){
				list_command(dataFD);
			}
			else if((strcmp(command_inputs[1], "-g")) == 0){
			    	get_file_command(command_inputs[2], dataFD, controlFD);
			}
			
			close(dataFD);
		}	
		
		
		// Cleanup
		if(get_host_info_return == 0 && client_host_name != NULL) { free(client_host_name); }
		close(controlFD);
	}

	deallocate_commands(&command_inputs);	
	close(listenSocketFD);

	return 0;
}

/******************************************************************************
 *	   Description: clears out contents of command_inputs
 *	    Parameters: receives address of array of c-strings
 *	       Returns: 0
 *	Pre-conditions: none command_inputs is an address of a 2d array of chars
 *     Post-conditions: contents of command_inputs cleared and all indices set to null
******************************************************************************/
void clear_command_inputs(char*** command_inputs){
	int i, j;
	for(i = 0; i < 3; i++){
		for(j = 0; j < 32; j++){
			(*command_inputs)[i][j] = '\0';
		}
	}
}

/******************************************************************************
 *	   Description: Reads lines from a file and appends it to a string to send
 *	   		over the socket connection described by the file descriptor dataFD
 *	    Parameters: receives the name of a file stored in filename and a socket 
 *	    		file descriptor
 *	       Returns: boolean int
 *	Pre-conditions: filename represents a file in the local directory
 *			dataFD is a socket file descriptor
 *     Post-conditions: bytes from the file filename are sent over the socket 
 *     			file descriptor dataFD
******************************************************************************/
int send_file(char* filename, int dataFD){
	FILE* fptr;
	int iter;
	char write_buffer[1024];
	char read_buffer[256];
	int read_count = 0;
	
	fptr = fopen(filename, "r");
	if (fptr == NULL){
		perror("Error reading file.");
		return 1;
	}
	memset(write_buffer, '\0', sizeof(write_buffer));
	while ( !feof(fptr) ) {		//check that we have no reached EOF
		
	    	if(fgets(read_buffer, 256, fptr) != NULL) {	//read first line of file
		    	//fprintf(stdout, "%s\n", read_buffer);
			read_count += strlen(read_buffer);	//increment number of bytes read
		    	//check if number of bytes read is greater than size of buffer to send over socket
			if( read_count < 1023){
				strcat(write_buffer, read_buffer);	//append to buffer
				memset(read_buffer, '\0', sizeof(read_buffer));	//clear buffer used to read bytes from file
			}
			else{	//number of bytes read is > 1023 so send what's in buffer and start over
				send(dataFD, write_buffer, strlen(write_buffer), 0);
				memset(write_buffer, '\0', sizeof(write_buffer));
				read_count = strlen(read_buffer);	//reset count of read bytes
				strcat(write_buffer, read_buffer);
				memset(read_buffer, '\0', sizeof(read_buffer));
			}
		} 
		
	}
	//if we reach EOF before filling write_buffer, send contents of write_buffer over socket
	if((strlen(write_buffer)) > 0){
		send(dataFD, write_buffer, strlen(write_buffer), 0);
	}

	return 0;
}


/******************************************************************************
 *	   Description: Opens the current directory to see if the file filename 
 *	   		exists. Then calls send_file to send the file over dataFD
 *	   		or an error over controlFD
 *	    Parameters: c string and two int socket file descriptors
 *	       Returns: boolean int for result of sending a whole file
 *	Pre-conditions: filename is not null and is a valid file, 
 *			dataFD & controlFD contain open socket file descriptors 
 *     Post-conditions: contents of a file are sent through dataFD
******************************************************************************/
int get_file_command(char* filename, int dataFD, int controlFD){
	char write_buffer[1024];
	struct dirent *contents;
	DIR *directory;
	int is_file = -1;

	memset(&write_buffer, '\0', sizeof(write_buffer));

	directory = opendir(".");
	
	//logic for getting file names obtained from https://pubs.opengroup.org/onlinepubs/7908799/xsh/readdir.html
	while((contents = readdir(directory)) != NULL){
		if((strcmp(filename, contents->d_name)) == 0){
			is_file = 1;
			break;
		}
	}

	closedir(directory);

	if(is_file == 1){
		//file exists, get contents and send through dataFD
		//fprintf(stdout, "Sending %s\n", filename);
	    	return (send_file(filename, dataFD));
	}
	else if (is_file == -1){
		//file does not exist, send error
		send(controlFD, "ERROR File not found", 20, 0);
		return 1;	
	}

	return 1;
}

/******************************************************************************
 *	   Description: Sends the contents of the current directory to the
 *	   		socket file descriptor dataFD
 *	    Parameters: receives an open socket file descriptor
 *	       Returns: 0
 *	Pre-conditions: socket file descriptor dataFD is open
 *     Post-conditions: file names of the current directory are sent to
 *     			the socket file descriptor dataFD
******************************************************************************/
int list_command(int dataFD){
	char write_buffer[1024];
    	struct dirent *contents;
	DIR *directory; 
	
	memset(&write_buffer, '\0', sizeof(write_buffer));

	directory = opendir(".");

	while((contents = readdir(directory)) != NULL){
		strcat(write_buffer, contents->d_name);
		strcat(write_buffer, " ");
	}

	send(dataFD, write_buffer, 1023, 0);

	closedir(directory);
	
	return 0;
}

/******************************************************************************
 *	   Description: sets up a socket connection with a listen socket at client_host_name, dataport
 *	    Parameters: receives 2 c strings, the host name and the port number in chars
 *	       Returns: socket file descriptor stored in an int
 *	Pre-conditions: client_host_name and dataport are not null
 *			both represent host name and port of a listen socket
 *     Post-conditions: a new socket connection is created and the file descriptor
 *     			is returned
 *
 *     	//logic obtained from example code supplied in CS344 & Beej's Guide		
******************************************************************************/
int setup_data_socket(char* client_host_name, char* dataport){
	struct sockaddr_in data_connection;
	struct hostent* HostInfo;
	int dataFD, portNumber;
/* SOCKET CONNECTION LOGIC */
	// Set up server address struct 
	memset((char*)&data_connection, '\0', sizeof(data_connection)); // Clear out the address struct
	portNumber = atoi(dataport);
	data_connection.sin_family = AF_INET;
	data_connection.sin_port = htons(portNumber); 
	
	HostInfo = gethostbyname(client_host_name);
	if (HostInfo == NULL) { perror("CLIENT: ERROR, no such host"); fflush(stderr); exit(2); }
	memcpy((char*)&data_connection.sin_addr.s_addr, (char*)HostInfo->h_addr, HostInfo->h_length);
	
	// Set up the listen socket
	dataFD = socket(AF_INET, SOCK_STREAM, 0); // Create the socket
	if (dataFD < 0) { perror("ftserver: ERROR opening data socket\n"); fflush(stderr); exit(2); }

	// Connect to server socket
	if (connect(dataFD, (struct sockaddr*)&data_connection, sizeof(data_connection)) < 0) { // Connect socket to address
		perror("ftserver: ERROR connecting to server"); fflush(stderr); exit(2);
	}
/* END SOCKET CONNECTION LOGIC */
	return dataFD;
}

/******************************************************************************
 *	   Description: Checks the command sent through the controlFD socket to
 *	   		be -g or -l
 *	    Parameters: receives address of command_inputs & the control socket 
 *	    		file descriptor
 *	       Returns: 0
 *	Pre-conditions: command_inputs is not null & controlFD is an open socket
 *     Post-conditions: Success or Error message is sent through controlFD socket
******************************************************************************/
int check_commands(char*** command_inputs, int controlFD){
    	if( (strcmp((*command_inputs)[1], "-g")) == 0 || (strcmp((*command_inputs)[1], "-l")) == 0){
		send(controlFD, "0", 1, 0);
		return 0;
	}
	else{
		send(controlFD, "ERROR Incorrect Command", 23, 0);
		return 1;
	}
}

/******************************************************************************
 *	   Description: allocates an an array of c strings
 *	    Parameters: receives the address of a variable in main to store new allocation
 *	       Returns: 0 
 *	Pre-conditions: none
 *     Post-conditions: memory allocated and stored in address stored in command_inputs
******************************************************************************/
int allocate_commands(char*** command_inputs){
	int i;

    	(*command_inputs) = (char **)malloc(3*sizeof(char *));
	if(command_inputs == NULL){ fprintf(stderr, "Out of Memory\n"); exit(1);}

	for(i = 0; i < 3; i++){
		(*command_inputs)[i] = (char *)malloc(32*sizeof(char));
	}
	
	return 0;
}

/******************************************************************************
 *	   Description: Frees the memory allocated for command_inputs
 *	    Parameters: receives a pointer to an array of c strings
 *	       Returns: none
 *	Pre-conditions: command_inputs points to an array of c strings
 *     Post-conditions: memory allocated for command_inputs is freed and the 
 *     			pointer is set to NULL
******************************************************************************/
void deallocate_commands(char*** command_inputs){
	int i;

    	if((*command_inputs) != NULL) { 
		for(i = 2; i >= 0; i--){
			free((*command_inputs)[i]);
		}
		free((*command_inputs));
		(*command_inputs) = NULL;	
	}
}

/******************************************************************************
 *	   Description: receives a string of characters from socket connection
 *	   		and stores the separate words in an array
 *	    Parameters: address of an array of c strings
 *	       Returns: 0
 *	Pre-conditions: command_inputs points to an array of strings and
 *			read_buffer is not null and contains a string
 *     Post-conditions: command_inputs[0] == dataport 
			command_inputs[1] == command 
			command_inputs[2] == filename (if it exists) 
******************************************************************************/
int parse_command_input(char*** command_inputs, char* read_buffer){
	int i, j = 0, command = 0;
	
	for(i = 0; read_buffer[i] != '\0'; i++){ //iterate through read buffer until reaching null char
		if(command == 3) { break; }
	    	if(read_buffer[i] != ' '){
		    		//if we're not at a space character, move contents into current command
				(*command_inputs)[command][j] = read_buffer[i];
				j++;
		}
		else if (read_buffer[i] == ' '){ 
		     command++;	//found space move to next command
		     j = 0; 	//reset char count
		}
	}

	return 0;
}


/******************************************************************************
 *	   Description: Prints the host name of the passed client address struct
 *	    Parameters: receives address of the string client_host_name
 *	       Returns: boolean int
 *	Pre-conditions: clientAddress struct is not null
 *     Post-conditions: client_host_name contains the host name of the client host
 *     			memory allocated for client_host_name
******************************************************************************/
int print_connection_info(char** client_host_name, struct sockaddr_in clientAddress){
	struct hostent *hostp;
	int client_host_length = 0;

	hostp = gethostbyaddr((const char *)&clientAddress.sin_addr.s_addr, 
		sizeof(clientAddress.sin_addr.s_addr), AF_INET);
	if(hostp == NULL){
		perror("ftserver ERROR on gethostbyaddr");
		return 1;
	}

	client_host_length = strlen(hostp->h_name);
	(*client_host_name) = (char*) malloc((client_host_length+1) * sizeof(char));
	memset((*client_host_name), '\0', sizeof((*client_host_name)));
	strcpy((*client_host_name), hostp->h_name);
	fprintf(stdout, "ftserver: Received connection from %s\n", (*client_host_name));

	return 0;
}

/******************************************************************************
 *	   Description: Sets up a socket connection with a server on the given
 *	   		port
 *	    Parameters: address of int, an array of c style strings 
 *	       Returns: socket file descriptor number for this listen socket
 *	Pre-conditions: argv is not null
 *     Post-conditions: socket file descriptor is returned as an int
 *     
 *     Logic and some syntax obtained from Beej's Guide 
 *     	https://beej.us/guide/bgnet/html/single/bgnet.html#simpleserver 
******************************************************************************/
int setup_listen_socket(int* listenSocketFD, char* argv[]){
	int portNumber;
	struct sockaddr_in serverAddress;
	int one = 1;

	memset(&serverAddress, '\0', sizeof(serverAddress));
	portNumber = atoi(argv[1]);
	serverAddress.sin_family = AF_INET;	// IPv4 socket
	serverAddress.sin_port = htons(portNumber); // store network order port number
	serverAddress.sin_addr.s_addr = htonl(INADDR_ANY); // Any address allowed for connection
	
	(*listenSocketFD) = socket(AF_INET, SOCK_STREAM, 0);
	
	if((*listenSocketFD) < 0){
		perror("ERROR opening socket"); exit(2);
	}
	// allow reuse of port number
	// Syntax from https://beej.us/guide/bgnet/html/single/bgnet.html#setsockoptman
	setsockopt((*listenSocketFD), SOL_SOCKET, SO_REUSEADDR, (const void *)&one, sizeof(int));
	if ( (bind((*listenSocketFD), (struct sockaddr *)&serverAddress, sizeof(serverAddress))) < 0){
		perror("ERROR on bind"); exit(2);
	}
	listen((*listenSocketFD), 1);
	
	return 0;
}


#endif
