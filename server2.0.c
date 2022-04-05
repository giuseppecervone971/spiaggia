/*
Authors: Nicola Adami and Giuseppe Cervone

Description: A server application to handle a beach's reservation.
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <strings.h>
#include <string.h>
#include <pthread.h>
#include "thpool.h"
#include <semaphore.h>

#define PORT 1111

/*Dimension for the beach, defining overall number of umbrellas, number of umbrellas per row, and number of rows*/
#define DIMBEACH 30
#define ROWS 3
#define UMBRELLAPERROW 10


#define MAX 70 //Size of the buffer
#define todayDate 190717
#define clients 16 //Max number of client

//Abstract data structure for the reservation
typedef struct {
    int startReservation;
    int endReservation;
} Data;
typedef struct node {
  Data reservation;
  struct node* next;
} Node;
typedef Node* List;

//Each Umbrella has an id and a list of reservation and a mutex
struct Umbrella {
    int idUmbrella;
    List Reservations;
};
struct Umbrella Beach[DIMBEACH];
sem_t umbrellaMutex[DIMBEACH];

threadpool thpool; 
int connfds[clients];

//Global variables
int go = 1;


int availableToday = DIMBEACH;
int availablePerRow[ROWS];

char buff[MAX];
int sockfd;
int connfd;

FILE *booking;

//Function declarations for everything that has to do with the list
void newList(List* l);
void insOrd(List* l, Data d);
List* ricercaStart(List *l, Data d);
void insTesta(List* l, Data d);
int elim1(List* l, int d, int deleteUmbrella);
List* ricercaElim(List* l, int d, int deleteUmbrella);
void elimTesta(List* l);
int searchReservation(List l, Data d);
void book(List* l, Data d);
List* ricercaBook(List* l, Data d);


void sighand (int sig) {    // When CTRL-c is pressed, server is closed in a "controlled" manner
    
    if ( sig == SIGINT ) {
       /* fprintf(stdout, "Server closing...\n");*/
        //gothread = 0;
    }


    int j = 0;
    while (j<clients) {
        if(connfds[j]!=0) {
            strcpy(buff,"EXIT");
            write(connfds[j], buff, sizeof(buff));
        }
        j++;
    }

    close(sockfd);
    close(connfd);

    //When the server can close, I open the booking file again and update it with all the reservations
    if((booking = fopen("logUmbrellas.txt", "wt")) == 0) {   
        fprintf(stderr, "Error File Open...\n");   
        exit(-1);
    } 

    for(int i = 0; i<DIMBEACH; i++) {
        while(Beach[i].Reservations) {
            fprintf(booking, "%d %d %d\n", Beach[i].idUmbrella, Beach[i].Reservations->reservation.startReservation, Beach[i].Reservations->reservation.endReservation);
            Beach[i].Reservations= Beach[i].Reservations->next;
        }
    }

    fclose(booking);

   /*fprintf(stdout, "Server Updated... Closing Server\n");*/

    exit(1);
    
    return;
}

void communication (void * connfd) {

    char *token, *token1, *token2, *token3;
    int umbrella;
    int sock = *(int*)connfd;
    Data d; 
    int gothread = 1;

    while (gothread) {
        memset(buff,'\0',sizeof(buff));
        read(sock, buff, sizeof(buff)); 
        token=strtok(buff," ");

        if((strncmp("CANCEL",token,6))==0) {  
            if(((token1=strtok(NULL," "))==NULL) || ((token2=strtok(NULL," "))==NULL)) {
                strcpy(buff,"INCORRECT ARGUMENTS\n");
                write(sock, buff, sizeof(buff));
            } else {    
                int deleteUmbrella=atoi(token1);
                int dateEnd=atoi(token2);
                if(deleteUmbrella>DIMBEACH) {
                    strcpy(buff,"CANCEL FAILED\n");
                    write(sock, buff, sizeof(buff));
                } else {
                    if (!(elim1(&Beach[deleteUmbrella-1].Reservations, dateEnd, deleteUmbrella))) {
                        strcpy(buff,"CANCEL FAILED\n");
                        write(sock, buff, sizeof(buff));
                    } else {
                        strcpy(buff,"CANCEL OK\n");    
                        write(sock, buff, sizeof(buff));
                    }
                }
            }
              
        } else if(strncmp("AVAILABLE",token,9)==0) {
            if((token1=strtok(NULL," "))==NULL) { 
                sprintf(buff,"AVAILABLE %d\n", availableToday);
                write(sock, buff, sizeof(buff));
            } else {
                int row = atoi(token1);
                if (row>ROWS) {
                    sprintf(buff,"ROW N/A");
                    write(sock, buff, sizeof(buff));
                } else {  
                    sprintf(buff,"AVAILABLE %d\n", availablePerRow[row-1]);
                    write(sock, buff, sizeof(buff));
                }
            }
                
        } else if (strncmp("BOOK",token,4)==0) {   
            if((token1=strtok(NULL," "))==NULL) {
                strcpy(buff, "INCORRECT ARGUMENTS\n");
                write(sock, buff, sizeof(buff)); 
            } else if ((token2=strtok(NULL," "))==NULL) {
                umbrella=atoi(token1);
                d.startReservation=todayDate;
                d.endReservation=todayDate;

                if (umbrella>DIMBEACH) {
                    strcpy(buff, "INCORRECT ARGUMENTS\n");
                    write(sock, buff, sizeof(buff));
                } else { 
                    sem_wait(&umbrellaMutex[umbrella-1]);
                    if ((searchReservation(Beach[umbrella-1].Reservations, d))) {
                        strcpy(buff, "NAVAILABLE\n");
                        write(sock, buff, sizeof(buff));
                        sem_post(&umbrellaMutex[umbrella-1]);
                        gothread = 0;    // END OF CONNECTION                                        
                    } else {
                        strcpy(buff, "SERVER AVAILABLE, CONFIRM OR CANCEL?\n");
                        write(sock, buff, sizeof(buff));

                        read(sock, buff, sizeof(buff));
                        if (strncmp("CANCEL",buff,6)==0) {
                            strcpy(buff, "BOOKING CANCELED\n");
                            write(sock, buff, sizeof(buff));
                            sem_post(&umbrellaMutex[umbrella-1]);
                        }  else if (strncmp("CONFIRM",buff,7)==0) {
                            book(&Beach[umbrella-1].Reservations, d);
                            availableToday--;
                            availablePerRow[(umbrella-1) / UMBRELLAPERROW]--;
                            strcpy(buff, "BOOK CONFIRMED\n");
                            write(sock, buff, sizeof(buff));
                            sem_post(&umbrellaMutex[umbrella-1]);
                        } else {
                            strcpy(buff, "COMMAND NOT FOUND; POSSIBLE COMMANDS ARE: \nCONFIRM\nCANCEL\n");
                            write(sock, buff, sizeof(buff));
                            sem_post(&umbrellaMutex[umbrella-1]);
                    
                        } 
                    }
                }
            } else if ((token3=strtok(NULL," "))==NULL) {
                umbrella=atoi(token1);
                d.startReservation=todayDate;
                d.endReservation=atoi(token2);

                if (umbrella>DIMBEACH || d.startReservation > d.endReservation) {
                    strcpy(buff, "INCORRECT ARGUMENTS\n");
                    write(sock, buff, sizeof(buff));
                } else {

                    sem_wait(&umbrellaMutex[umbrella-1]);
                    if ((searchReservation(Beach[umbrella-1].Reservations, d))) {
                        strcpy(buff, "NAVAILABLE");
                        write(sock, buff, sizeof(buff));
                        sem_post(&umbrellaMutex[umbrella-1]); 
                        gothread = 0; 
                    } else {
                        strcpy(buff,"SERVER AVAILABLE, CONFIRM or CANCEL?");
                        write(sock, buff, sizeof(buff));
                    
                        read(sock, buff, sizeof(buff));
                        if (strncmp("CANCEL",buff,6)==0) {
                            strcpy(buff, "BOOKING CANCELED\n");
                            write(sock, buff, sizeof(buff));
                            sem_post(&umbrellaMutex[umbrella-1]);
                        }  else if (strncmp("CONFIRM",buff,7)==0) {
                            book(&Beach[umbrella-1].Reservations, d);
                            availableToday--;
                            availablePerRow[(umbrella-1) / UMBRELLAPERROW]--;
                            strcpy(buff, "BOOK CONFIRMED\n");
                            write(sock, buff, sizeof(buff));
                            sem_post(&umbrellaMutex[umbrella-1]);
                        } else {
                            strcpy(buff, "COMMAND NOT FOUND; POSSIBLE COMMANDS ARE: \nCONFIRM\nCANCEL\n");
                            write(sock, buff, sizeof(buff));
                            sem_post(&umbrellaMutex[umbrella-1]);
                        } 
                    }
                }    
            } else {
                umbrella=atoi(token1);
                d.startReservation=atoi(token2);
                d.endReservation=atoi(token3);

                if (umbrella>DIMBEACH || d.startReservation > d.endReservation || todayDate>d.endReservation) {
                    strcpy(buff, "INCORRECT ARGUMENTS\n");
                    write(sock, buff, sizeof(buff));
                } else {
                    sem_wait(&umbrellaMutex[umbrella-1]);
                    if ((searchReservation(Beach[umbrella-1].Reservations, d))) {
                        strcpy(buff, "NAVAILABLE\n");
                        write(sock, buff, sizeof(buff));
                        sem_post(&umbrellaMutex[umbrella-1]);
                        gothread = 0;
                    } else {
        
                        strcpy(buff,"SERVER AVAILABLE, CONFIRM or CANCEL?");
                        write(sock, buff, sizeof(buff));
                    
                        read(sock, buff, sizeof(buff));
                        if (strncmp("CANCEL",buff,6)==0) {
                            strcpy(buff, "BOOKING CANCELED\n");
                            write(sock, buff, sizeof(buff));
                            sem_post(&umbrellaMutex[umbrella-1]);
                        } else if (strncmp("CONFIRM",buff,7)==0) {
                            book(&Beach[umbrella-1].Reservations, d);

                            if ((todayDate >= d.startReservation) && (todayDate <= d.endReservation)) {
                                availableToday--;
                                availablePerRow[(umbrella-1) / UMBRELLAPERROW]--;
                            }

                            strcpy(buff, "BOOK CONFIRMED\n");
                            write(sock, buff, sizeof(buff));
                            sem_post(&umbrellaMutex[umbrella-1]);
                        } else {
                            strcpy(buff, "COMMAND NOT FOUND; POSSIBLE COMMANDS ARE: \nCONFIRM\nCANCEL\n");
                            write(sock, buff, sizeof(buff));
                            sem_post(&umbrellaMutex[umbrella-1]);
                        }    
                    }                    
                }
            }
        } else if (strncmp("EXIT",token,4)==0) {
            strcpy(buff, "EXIT\n");
            write(sock, buff, sizeof(buff));
            gothread = 0;
            
            int j = 0;

            while (j<clients) {
                
                if(connfds[j]==sock) {
                    connfds[j] = 0;
                    break;
                }
                j++;    
            }

        } else {
            strcpy(buff, "COMMAND N/A. POSSIBLE COMMANDS ARE: \nBOOK\nAVAILABLE\nCANCEL\nEXIT\n");
            write(sock, buff, sizeof(buff)); 
        }
    
    }
    close(sock);
}

int main (int argc, char ** argv) {
    
    pid_t pid = fork();
    
    if (pid>0) {
        exit(0);
    }
    

    int id; //declarations used to scan the file
    Data d;
    struct sockaddr_in servaddr, cliaddr;
    socklen_t clilen;    
    
    thpool = thpool_init(clients); 
    //initializing threadpool, each thread in the threadpool is a client connecting to the server 

    //Error control for server executable and log file
    if (argc !=2) {   
        fprintf(stderr, "Not the right number of arguments...\n");   
        exit(-1);                 
    } 

    //Error control for signal mapping
    if (signal(SIGINT,  sighand) == SIG_ERR ) {
        fprintf(stderr, "signal failed...\n");
    }

    //Error control 
    if ((booking = fopen(argv[1], "rt")) == 0) {  
        fprintf(stderr, "Error File Open %s...\n", argv[1]);     
        exit(-1);
    }

    //Initializing the list of the reservation
    for (int i = 0; i<DIMBEACH; i++) {
        newList(&Beach[i].Reservations);
        Beach[i].idUmbrella=i+1;
        availablePerRow[i/UMBRELLAPERROW]=UMBRELLAPERROW;   
    }

    for (int i = 0; i < clients; i++) {
        connfds[i] = 0;
    }
    
    //Loop filling the umbrella data structure array with the logFile.
    while (fscanf(booking, "%d%d%d", &id, &d.startReservation, &d.endReservation)!= EOF) {
        if (todayDate >= d.startReservation && todayDate <= d.endReservation) {
            availableToday--;
            availablePerRow[(id-1) / UMBRELLAPERROW]--;
        }
        insOrd(&Beach[id-1].Reservations, d);
    }

    for (int i =0; i < DIMBEACH; i++) {
        sem_init(&umbrellaMutex[i], 0, 1);
    }

    fclose(booking);

    //Creation of the socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	if (sockfd == -1) { 
		fprintf(stderr, "Socket creation failed...\n"); 
		exit(-1); 
	} /*else {
		fprintf(stdout, "Socket successfully created...\n"); 
    }
    */

	memset(&servaddr, '\0', sizeof(servaddr)); 

	//Assigning local IP to socket
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY); 
	servaddr.sin_port = htons(PORT); 

	//Socket Bind to IP
	if ((bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) { 
		fprintf(stderr, "socket bind failed...\n"); 
		exit(-1); 
	} /*else {
		fprintf(stdout, "Socket successfully binded...\n"); 
    }
    */
	// Now server is ready to listen and verification 
	if ((listen(sockfd, clients)) != 0) { 
		fprintf(stderr, "Listen failed...\n"); 
		exit(-1); 
	}/* else { 
		fprintf(stdout, "Server listening...\n"); 
    }
    */

   //posto per un ipotetico salvataggio periodico del file. thpool_add_work(thpool, (void*)journaling, NULL); 
   //prototipo void journaling:
   //while (go){ sleep (60); //update file; }
    
    while (go)  { //Client thread connection initialized only if the conditions are good
        clilen = sizeof(cliaddr);
        
        connfd = accept(sockfd, (struct sockaddr *) &cliaddr, &clilen);  
        
        int j=0;
        while (j<clients) {
            if(connfds[j]!=0) {
                j++;
            } else {
                connfds[j] = connfd;
                break;
            } 
        }

  
        if (connfd < 0){
            fprintf(stderr, "Error on Accept...\n");  
        } 

        if (go) {

            read(connfd, buff, sizeof(buff));
            memset(buff, '\0', sizeof(buff)); 
        
            if (thpool_num_threads_working(thpool)==clients) {  //if it's full, we can't add another client
                strcpy(buff,"NOK\n");
                write(connfd, buff, sizeof(buff));
            } else if (availableToday==0) {                        //in this case we can still cancel a reservation  
                sprintf(buff,"NAVAILABLE\n");
                write(connfd, buff, sizeof(buff));
                thpool_add_work(thpool, (void*)communication, (void*) &connfd); 
            } else { 
                sprintf(buff,"OK\n");
                write(connfd, buff, sizeof(buff));
                thpool_add_work(thpool, (void*)communication, (void*) &connfd); 
            }

        } 
        memset(buff, '\0', sizeof(buff)); 
    }
    thpool_wait(thpool);

}
//Program closing handled in sighandler

// List functions 
void newList(List* l) {
  *l = NULL;
}

void insOrd(List* l, Data d) {
    l = ricercaStart(l, d);
    insTesta(l, d);
}

List* ricercaStart(List *l, Data d) {
    while(*l) {
        if ( ((*l)->reservation.startReservation) > d.startReservation && (((*l)->reservation.endReservation) > d.endReservation)) {
            break;
        }
        l = &(*l)->next;
    }
    return l;
}

void insTesta(List* l, Data d) {
    Node* aux = (Node*)malloc(sizeof(Node));
    aux->reservation = d;
    aux->next = *l;
    *l = aux;
}

int elim1(List* l, int d, int deleteUmbrella) {
    l = ricercaElim(l, d, deleteUmbrella);
    if (*l) {
        elimTesta(l); 
        return 1;
    } else {
        return 0;
    }
}

List* ricercaElim(List* l, int d, int deleteUmbrella) {
    while (*l) {
        if ((*l)->reservation.endReservation == d) {
            if ((todayDate >= (*l)->reservation.startReservation) && (todayDate <= (*l)->reservation.endReservation)) {
                availableToday++;
                availablePerRow[(deleteUmbrella-1) / UMBRELLAPERROW]++;
            }
            break;
        }
        l = &(*l)->next;
    }
    return l;
}

void elimTesta(List* l) {
    Node* aux = *l;
    *l = (*l)->next;
    free(aux);
}

int searchReservation(List l, Data d) {
    while (l) {
        if( (d.startReservation<=l->reservation.startReservation && d.endReservation >= l->reservation.startReservation) || (d.startReservation >= l->reservation.startReservation && d.startReservation<= l->reservation.endReservation)) {
            return 1;
        }
        l = l->next;
    }
    return 0;
}

void book(List* l, Data d) {
    l = ricercaBook(l, d);
    insTesta(l, d);
}

List* ricercaBook(List* l, Data d) {
    while (*l) {
        if ((*l)->reservation.startReservation > d.startReservation) {
        break;
        }
        l = &(*l)->next;
    }
    return l;
}
