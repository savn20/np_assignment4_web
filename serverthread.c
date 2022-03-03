#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <pthread.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "threadpool/threadpool.h"

#define DEBUG
#define BACKLOG 1000
#define PATH_SIZE 1000
#define SOCKET_FAILURE -1
#define MAXDATASIZE 1400
#define POOLSIZE 50

int totalClients = 0;
char buffer[MAXDATASIZE];


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

  printf("serving request for %d\n",clientSocket);

  char *accessMethod, *fileName;

  size_t bytes;
  int bytesRead = 0;

  verify(bytes = read(clientSocket, buffer, MAXDATASIZE));
  buffer[bytes - 1] = '\0';

  printf("file requested: %s\n", buffer);
  fflush(stdout);

  accessMethod = strtok(buffer, " ");

  if (!(strcmp(accessMethod, "GET") == 0 || strcmp(accessMethod, "HEAD") == 0)) {
    perror("error: can't accept access method");
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
    printf("error: oops no file found\n");
    close(clientSocket);
    return NULL;
  }

  while (bytesRead = fread(buffer, 1, MAXDATASIZE, fp)) {
    printf("sending %d bytes\n", bytesRead);
    write(clientSocket, buffer, bytesRead);
  }

  close(clientSocket);
  fclose(fp);
  return NULL;
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
    perror("usage: serverfork <ip>:<port>\n"
         "program terminated due to wrong usage\n");

    exit(-1);
  }

  char seperator[] = ":";
  char* serverIp = strtok(argv[1], seperator);
  char* destPort = strtok(NULL, seperator);
  int serverPort = atoi(destPort);
  
  int listenFd = -1, // for server socket
      acceptFd = -1, // for client socket 
      reuseAddress = 1;

  verify(listenFd = socket(AF_INET, SOCK_STREAM, 0));

  /* initialize the socket addresses */
  struct sockaddr_in serverAddress, clientAddress;
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

  struct thread_pool* pool = pool_init(4);

  while (1) {
    // parent process waiting to accept a new connection
    printf("\n*****server waiting for new client connection:*****\n");
    clientAddressLength = sizeof(clientAddress);
    acceptFd = accept(listenFd, (struct sockaddr *)&clientAddress, &clientAddressLength);

    totalClients++; 
    printf("totalClient = %d accept = %d listenFd = %d\n", totalClients, acceptFd, listenFd);
    
    int *client = (int *)malloc(sizeof(int));
    *client = acceptFd;
    
    pool_add_task(pool, serveRequest, client);
  }
  
  return (0);
}