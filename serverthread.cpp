#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <pthread.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <climits>

#define DEBUG
#define BACKLOG INT_MAX
#define PATH_SIZE 1000
#define SOCKET_FAILURE -1
#define MAXDATASIZE 1400
#define POOLSIZE 50

using namespace std;

struct node {
    struct node* next;
    int* client_socket;
};

typedef struct node node_t;

node_t *head = NULL;
node_t *tail = NULL;

pthread_t clientPool[POOLSIZE];
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t terenary = PTHREAD_COND_INITIALIZER;
int totalClients = 0;

void enqueue(int *client_socket) {
    node_t *newnode = (node_t*) malloc(sizeof(node_t));
    newnode->client_socket = client_socket;
    newnode->next = NULL;

    if (tail == NULL) 
        head = newnode;
    else 
        tail->next = newnode;
    
    tail = newnode;   
}

// returns NULL if the queue is empty.
// Returns the pointer to a client_socket, if there is one to get
int* dequeue() {
    if (head == NULL) 
         return NULL;
        
    int *result = head->client_socket;
    node_t *temp = head;
    head = head->next;

    if (head == NULL) {tail = NULL;}
    free(temp);
    return result;
}

void verify(int hasError) {
  if (hasError == SOCKET_FAILURE) {
    perror("error: something went wrong dealing with sockets\n");
    exit(-1);
  }
}

void httpHeader(int handler, int errorCode) {
    char header[1000] = {0};
    if (errorCode == 0)
        sprintf(header, "HTTP/1.0 200 OK\r\n\r\n");
    else if (errorCode == 1)
        sprintf(header, "HTTP/1.0 403 Forbidden\r\n\r\n");
    else
        sprintf(header, "HTTP/1.0 404 Not Found\r\n\r\n");

    send(handler, header, strlen(header), 0);
}

void *serveRequest(void* client) {
  int clientSocket = *(int *)client;
  free(client);

  cout << "serving request for " << clientSocket << endl;

  char *buffer = (char *)malloc(MAXDATASIZE);
  char *accessMethod, *fileName;

  size_t bytes;
  int bytesRead = 0;

  verify(bytes = read(clientSocket, buffer, MAXDATASIZE));
  buffer[bytes - 1] = '\0';

  printf("file requested: %s\n", buffer);
  fflush(stdout);

  accessMethod = strtok(buffer, " ");

  if (!(strcmp(accessMethod, "GET") == 0 || strcmp(accessMethod, "HEAD") == 0)) {
    cerr << "error: can't accept access method " << accessMethod << endl;
    close(clientSocket);
    return NULL;
  }

  fileName = strtok(NULL, " ");

  if (fileName[0] == '/')
    fileName++;

  if (access(fileName, F_OK) != 0) {
    httpHeader(clientSocket, 2);
    return NULL;
  }
  else if (access(fileName, R_OK) != 0) {
    httpHeader(clientSocket, 1);
    return NULL;
  }
  else
    httpHeader(clientSocket, 0);

  FILE *fp = fopen(fileName, "rb");
  if (fp == NULL) {
    cerr << "error: oops no file found\n";
    close(clientSocket);
    return NULL;
  }

  while (bytesRead = fread(buffer, 1, MAXDATASIZE, fp)) {
    cout << "sending " << bytesRead << " bytes\n";
    write(clientSocket, buffer, bytesRead);
  }

  free(buffer);
  close(clientSocket);
  fclose(fp);
  return NULL;
}

void* handleIncomingRequest(void* args){
  while (true) {
    int* newClient;
    strerror(pthread_mutex_lock(&mutex));
    strerror(pthread_cond_wait(&terenary, &mutex));
    newClient = dequeue();
    strerror(pthread_mutex_unlock(&mutex));

    if(newClient != NULL){
      serveRequest(newClient);
    }
  }
}

int main(int argc, char *argv[]) {
  // disables debugging when there's no DEBUG macro defined
#ifndef DEBUG
  cout.setstate(ios_base::failbit);
  cerr.setstate(ios_base::failbit);
#endif

  /*************************************/
  /*  getting ip and port from args   */
  /***********************************/
  if (argc != 2) {
    cerr << "usage: serverfork <ip>:<port>\n"
         << "program terminated due to wrong usage" << endl;

    exit(-1);
  }

  char seperator[] = ":";
  char* serverIp = strtok(argv[1], seperator);
  char* destPort = strtok(NULL, seperator);
  int serverPort = atoi(destPort);
  
  int listenFd = -1, // for server socket
      acceptFd = -1, // for client socket 
      reuseAddress = 1;

  for (int i = 0; i < POOLSIZE; i++) 
    pthread_create(&clientPool[i], NULL, handleIncomingRequest, NULL);

  verify(listenFd = socket(AF_INET, SOCK_STREAM, 0));

  /* initialize the socket addresses */
  sockaddr_in serverAddress, clientAddress;
  memset(&serverAddress, 0, sizeof(serverAddress));
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_port = htons(serverPort);
  serverAddress.sin_addr.s_addr = inet_addr(serverIp);

  socklen_t clientAddressLength = sizeof(struct sockaddr_in);

  verify(setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &reuseAddress, sizeof(int)));
  
  /* bind the socket with the server address and port */
  verify(bind(listenFd, (struct sockaddr *)&serverAddress, sizeof(serverAddress)));

  /* listen for connection from client */
  verify(listen(listenFd, BACKLOG));

  while (1) {
    // parent process waiting to accept a new connection
    cout << "\n*****server waiting for new client connection:*****\n";
    clientAddressLength = sizeof(clientAddress);
    acceptFd = accept(listenFd, (struct sockaddr *)&clientAddress, &clientAddressLength);

    totalClients++; 
    cout << "totalClient = " << totalClients << " accept = " << acceptFd << " listenFd = " << listenFd << endl;
    
    int *client = (int *)malloc(sizeof(int));
    *client = acceptFd;
    
    strerror(pthread_mutex_lock(&mutex));
    enqueue(client);
    strerror(pthread_cond_signal(&terenary));
    strerror(pthread_mutex_unlock(&mutex));
  }
  
  return (0);
}