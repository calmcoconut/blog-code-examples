#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

int setupTcpClientSocket(const char *host, const char *service)
{
  struct addrinfo addrCriteria; // addrCriteria used to filter on available addresses
  memset(&addrCriteria, 0, sizeof(addrCriteria));
  addrCriteria.ai_family = AF_UNSPEC; // dont care if ipv4 or ipv6
  addrCriteria.ai_socktype = SOCK_STREAM; // want a TCP
  addrCriteria.ai_protocol = IPPROTO_TCP; // again, tcp protocol

  struct addrinfo *servAddr; // will hold the returned linked list of addresses
  int rtnVal = getaddrinfo(host, service, &addrCriteria, &servAddr); // getaddrinfo is your friend! very important function
  if (rtnVal != 0) {
    fputs("getaddrinfo() failed", stderr);
    exit(1);
  }
  int sock = -1; // default a sock as negative, so we know if it does not bind
  for (struct addrinfo *addr = servAddr; addr != NULL; addr = addr->ai_next) {
    sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol); // try to bind for each addr in linked-list until successful
    if (sock < 0)
      continue;
    if (connect(sock, addr->ai_addr, addr->ai_addrlen) == 0)
      break; // break out of loop if success
    close(sock);
    sock = -1;  // finished loop without a success! bad!
  }
  freeaddrinfo(servAddr); // remove the linked list from memory since we're finished with it
  return sock; // our new socket
}

int main(int argc, char *argv[])
{
  // we want 4 args the program name and the last 1 is optional
  if (argc < 3 || argc > 4){
    fputs("Parameter(s): <Server Address/Name> <Echo String> [<Server Port/Service>]", stderr);
    exit(-1);
    }

  char *server = argv[1]; // first argument, after the program name, is the server address/name
  char *echoString = argv[2]; // second arg: string to echo
  // third argument is optional server/port number
  char *service = (argc == 4) ? argv[3] : "echo";

  int sock = setupTcpClientSocket(server, service);
  if (sock < 0) {
    fputs("setupTcpClientSocket() failed: unable to connected", stderr);
    exit(1);
  }

  size_t echoStringLen = strlen(echoString);
  ssize_t numBytes = send(sock, echoString, echoStringLen, 0);
  if (numBytes < 0) {
    fputs("send() failed", stderr);
    exit(1);
  }
  else if (numBytes != echoStringLen) {
    fputs("send() sent an unexpected number of bytes", stderr);
    exit(1);
  }

  // receive the same string from the server
  unsigned int totalBytesRecvd = 0;
  fputs("Received: ", stdout);
  while (totalBytesRecvd < echoStringLen) {
    char buffer[256];
    numBytes = recv(sock, buffer, 256-1, 0); // receive up to the buffer size - 1 for the \0 character
    if (numBytes < 0) {
      fputs("recv() failed", stderr);
      exit(1);
    }
    else if (numBytes == 0) {
      fputs("recv() connection closed prematurely", stderr);
      exit(1);
    }
    totalBytesRecvd += numBytes;
    buffer[numBytes] = '\0';
    fputs(buffer, stdout);
  }
  fputc('\n', stdout);
}
