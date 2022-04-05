/*
Authors: Nicola Adami and Giuseppe Cervone

Program description: A simple client interface.
*/


#include <netdb.h> 
#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <sys/socket.h> 
#include <signal.h>
#include <arpa/inet.h>
#include <unistd.h>
#define MAX 70
#define PORT 1111 
#define SA struct sockaddr



void sighand (int sig) {    //Controlling CTRL+c, we want to make sure the only time you can close the client is by using the command "EXIT"
    if ( sig == SIGINT ) {
    }
  return;
}

int main(int argc, char** argv) 
{ 
	int sockfd; 
	struct sockaddr_in servaddr; 
	int go=1;
	char buff[MAX]; 
	int n; 

	//Error control for the number of arguments 
    if (argc != 2) { 
        fprintf(stderr, "Error in the arguments of the program...\n");   
        exit(0);                                        
    } 
	//Error control for the argument "BOOK". In our implementation of the client, we want the use the word "BOOK" as a login word.
	if (strcmp(argv[1], "BOOK")) { 
        fprintf(stderr, "Error in the arguments of the program...\n");   
        exit(0);                                        
    }

	//Error control for signal mapping
    if (signal(SIGINT,  sighand) == SIG_ERR ) {
        fprintf(stderr, "Signal failed...\n");
    }


	//Socket creation	
	sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	if (sockfd == -1) { 
		fprintf(stderr, "Socket creation failed...\n"); 
		exit(0); 
	} else {
		fprintf(stdout, "Socket successfully created...\n"); 
	}

	
	// assign IP, PORT 
	memset(&servaddr, '\0' , sizeof(servaddr));
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = inet_addr("127.0.0.1"); 
	servaddr.sin_port = htons(PORT); 

	// connect the client socket to server socket 
	if (connect(sockfd, (SA*)&servaddr, sizeof(servaddr)) != 0) { 
		fprintf(stderr, "Connection with the server failed...\n"); 
		exit(0); 
	}



	//sending 'BOOK' command to buffer
	write(sockfd, argv[1], sizeof(argv[1])); //write on the socket
	memset(buff, '\0', sizeof(buff));  //empty buffer
	read(sockfd, buff, sizeof(buff));  //server response

	if ((strncmp(buff, "NOK", 3)) == 0) {  //if server response is NOK, then server is at max capacity and client closes.
		fprintf(stderr, "Client Exit...\n"); 
		exit(0);
		
	} else {
		fprintf(stdout, "Connected to server... List of commands:\n\n");
		fprintf(stdout, "BOOK $UMBRELLANUMBER\nBook $UMBRELLANUMBER $DATE\nBOOK $UMBRELLANUMBER $DATESTART $DATEEND\n");
		fprintf(stdout, "AVAILABLE\nAVAILABLE $ROW\n");
		fprintf(stdout, "CANCEL $UMBRELLANUMBER $DATEEND\n");
		fprintf(stdout, "EXIT\n");
	}
	
	/*
	Now the communication protocol starts, for every write to the server there's a read. If client writes EXIT, then server responds EXIT and the client closes.
	Otherwise client closes when NAVAILABLE is sent from server to client.
	*/


	while (go){ 

		memset(buff, '\0', sizeof(buff)); 
		n = 0; 

		while ((buff[n++] = getchar()) != '\n') {} //input to the server	    
		
		write(sockfd, buff, sizeof(buff));
		memset(buff, '\0', sizeof(buff));  

		read(sockfd, buff, sizeof(buff));  
		fprintf(stderr, "%s\n", buff);		


		if ((strncmp(buff, "NAVAILABLE", 10) == 0) || (strncmp(buff, "EXIT", 4) == 0)) {  
			fprintf(stdout, "Client Exit...\n"); 
			go=0; 
		} 

	} 
	// close the socket 
	//close(sockfd); 
	
	exit(1);
} 