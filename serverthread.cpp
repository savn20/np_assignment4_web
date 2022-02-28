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
#define BACKLOG 5
#define PATH_SIZE 1000
#define SOCKET_FAILURE -1
#define MAXDATASIZE INT_MAX

using namespace std;

int totalClients = 0;

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

void *serveRequest(void *client) {
  int clientSocket = *(int *)client;
  free(client);

  char *buffer = (char *)malloc(MAXDATASIZE);

  size_t bytes;
  int bytesRead = 0;

  char *accessMethod, *fileName;

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

  close(clientSocket);
  free(buffer);
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
    
    cout << "accept = " << acceptFd << " listenFd = " << listenFd << endl;

    pthread_t clientThread;
    int *client = (int *)malloc(sizeof(int));
    *client = acceptFd;
    pthread_create(&clientThread, NULL, serveRequest, client);
  }
  
  return (0);
}