#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

static const int MAXCON = 5;

void printSocketAddress(const struct sockaddr *address, FILE *stream) // this is just a function to print connection
{
  if (address == NULL || stream == NULL)
    return;

  void *numericAddress; // we're not sure of the size yet, so we use void
  char addrBuffer[INET6_ADDRSTRLEN]; // INET6_ADDRSTRLEN is the max len of an ipv6 connection, this is enough for both ipv6 and ipv4
  in_port_t port;
  switch(address->sa_family)
  {
    // need to convert to given address either to ipv4 or ipv6 compatible structure, using sockaddr_in or sockaddr_in6
    case AF_INET:
      numericAddress = &((struct sockaddr_in *) address)->sin_addr;
      port = ntohs(((struct sockaddr_in *) address)->sin_port); // network to host short
      break;
    case AF_INET6:
      numericAddress = &((struct sockaddr_in6 *) address)->sin6_addr;
      port = ntohs(((struct sockaddr_in6 *) address)->sin6_port); // network to host short
      break;
    default:
      fputs("[unknown type]", stream);
      return;
    }

    if (inet_ntop(address->sa_family, numericAddress, addrBuffer, sizeof(addrBuffer)) == NULL) // convert ipv4 or ipv6 address (inet) from bytes (network), to printable (string)
      fputs("[invalid address]", stream);
    else {
      fprintf(stream, "%s", addrBuffer); // print the address
      if (port != 0)
        fprintf(stream, "%u", port); // print the port
  }
}

void handleTcpClient (int clntSocket)
{
  char buffer[250]; // buffer one byte

  // receive message from client
  ssize_t numBytesRcvd = recv(clntSocket, buffer, 250, 0); // 256-1 is to make room for '\0'
  if (numBytesRcvd < 0){
    fputs("recv() failed", stderr);
    exit(1);
  }

  // send received string and receive again until end of stream
  while (numBytesRcvd > 0)
  {
    ssize_t numBytesSent = send(clntSocket, buffer, numBytesRcvd, 0);
    if (numBytesSent < 0){
      fputs("send() failed", stderr);
      exit(1);
    }
    else if (numBytesSent != numBytesRcvd){
      fputs("send() Sent unexpected number of bytes", stderr);
      exit(1);
    }
    // see if there is more data to receive
    numBytesRcvd = recv(clntSocket, buffer, 250, 0);
    if (numBytesRcvd < 0){
      fputs("recv() failed on second iteration", stderr);
      exit(1);
    }
  }
  close(clntSocket);
}

int setupTcpServerSocket(const char *service)
{
  struct addrinfo addrCriteria; // we use the addrinfo structure to hold the info... addrinfo is flexible
  memset(&addrCriteria, 0, sizeof(addrCriteria)); // clear out the struct
  addrCriteria.ai_family = AF_UNSPEC; // IP agnostic
  addrCriteria.ai_flags = AI_PASSIVE; // any
  addrCriteria.ai_socktype = SOCK_STREAM; // stream socket (TCP)
  addrCriteria.ai_protocol = IPPROTO_TCP; // TCP protocol

  struct addrinfo *servAddr; // this will hold all the returned addresses that match the criteria... linked list style
  int rtnVal = getaddrinfo(NULL, service, &addrCriteria, &servAddr); // getaddrinfo is our friend! works with both addresses and names
  if (rtnVal != 0)
  {
    fputs("get addrinfo() failed", stderr);
    fputs(gai_strerror(rtnVal), stderr);
    exit(1);
  }

  int servSock = -1;
  for (struct addrinfo *addr = servAddr; addr != NULL; addr = addr->ai_next) { // loop through all the returned addresses until one works
    servSock = socket(servAddr->ai_family, servAddr->ai_socktype, servAddr->ai_protocol); // try to bind the socket
    if (servSock < 0)
      continue; // socket creation failed, try the next one

    if ((bind(servSock, servAddr->ai_addr, servAddr->ai_addrlen) == 0) && (listen(servSock, MAXCON) == 0)) // if bound works and can listen to socket
    {
      struct sockaddr_storage localAddr; // sockaddr_storage is IP agnostic
      socklen_t addrSize = sizeof(localAddr);
      if (getsockname(servSock, (struct sockaddr *) &localAddr, &addrSize) < 0){
        fputs("getsockname() failed", stderr);
        exit(1);
      }
      fputs("binding to ", stdout);
      printSocketAddress((struct sockaddr *) &localAddr, stdout); // printing what address we bound to
      fputc('\n', stdout);
      break;
    }
    close(servSock);
    servSock = -1;
  }

  freeaddrinfo(servAddr);
  return servSock;
}

int acceptTcpConnection(int servSock)
{
  struct sockaddr_storage clntAddr; // client address
  socklen_t clntAddrLen = sizeof(clntAddr); // need the length of the client's address

  // wait for the client to connect
  int clntSock = accept(servSock, (struct sockaddr *) &clntAddr, &clntAddrLen); // accept a connection
  if (clntSock < 0) // negative return indicates failure
  {
    fputs("accept() failed", stderr);
    exit(-1);
  }
  fputs("handling client ", stdout);
  printSocketAddress((struct sockaddr *) &clntAddr, stdout); // just printing who connected
  fputc('\n', stdout); // break line
  return clntSock;
}

int main(int argc, char *argv[])
{
  if (argc != 2) { // we want two arguments, the program itself and the port
    fputs("Parameter(s): <Server Port/ Service", stderr);
    exit(1);
  }
  char *service = argv[1]; // the port we want to serve bind
  int servSock = setupTcpServerSocket(service); // let's make our server socket
  if (servSock < 0) {
    fputs("setupTcpServerSocket() failed", stderr);
    exit(1);
    }

  for (;;) { // run forever
    int clntSock = acceptTcpConnection(servSock);
    handleTcpClient(clntSock);
    close(clntSock);
  }
}
