#ifdef _WIN32
#pragma comment(lib,"ws2_32.lib")
// -lws2_32
#include <winsock2.h>
typedef int socklen_t;
#else
#include <sys/select.h> // select()
#include <sys/socket.h> // shutdown(), socket(), AF_INET, SOCK_STREAM, SHUT_WR, accept(), setsockopt()
#include <sys/types.h> // setsockopt()
#include <netinet/in.h> // struct sockaddr_in, htons()
#include <arpa/inet.h> // htons()
#include <unistd.h>
#define INVALID_SOCKET -1
typedef int SOCKET;
static int closesocket(int fd) {
  return close(fd);
}
#endif
#include <stdio.h>
#include <cstdlib>
#include <cctype>
#include <pthread.h>
#include <vector>
#include <sstream>

std::string onlyAscii(std::string some) {
  for (int i = 0; i < some.size(); i++) {
    if (!isascii(some[i])) {
      some[i] = '?';
    }
  }
  return some;
}

pthread_mutex_t Nthreads_mutex;
int Nthreads = 0;

struct ThreadParam {
  SOCKET socket;
};

void runCommand(std::string line, SOCKET socket) {
  std::stringstream ss;
  std::string cmd, file;
  ss << line;
  ss >> cmd >> file;
  cmd = onlyAscii(cmd);
  file = onlyAscii(file);
  printf("command: %s %s\n", cmd.c_str(), file.c_str());
  std::string out = "{\"progress\":100,\"songs\":[]}\r\n";
  send(socket, out.c_str(), out.size(), 0);
}

void *threadRunner(void *arg) {
  ThreadParam *param = (ThreadParam*)arg;
  std::stringstream linebuf;
  char buf[512];
  SOCKET socket = param->socket;
  for (;;) {
    int nrecv = recv(socket, buf, 512, 0);
    if (nrecv <= 0) break;
    for (int i = 0; i < nrecv; i++) {
      if (buf[i] == '\n') {
        runCommand(linebuf.str(), socket);
        linebuf = std::stringstream();
      }
      else linebuf.put(buf[i]);
    }
  }
  
  printf("tid %d disconnected\n", (int)pthread_self());
  closesocket(param->socket);
  delete param;
  pthread_mutex_lock(&Nthreads_mutex);
  Nthreads -= 1;
  pthread_mutex_unlock(&Nthreads_mutex);
  return 0;
}

int main(int argc, char const *argv[]) {
  SOCKET sockfd = INVALID_SOCKET;
  int port = 1605;
  int LISTENQ = 5;
  int MAX_THREADS = 2;

  int ret;

#ifdef _WIN32
  WSADATA wsaData = {0};
  ret = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (ret != 0) {
    printf("cannot WSAStartup\n");
    exit(1);
  }
#endif

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd == INVALID_SOCKET) {
    printf("cannot create socket\n");
    exit(1);
  }

  int yes[] = {1};
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char*)yes, sizeof yes) < 0) {
    fprintf( stderr, "failed to set reuse address\n");
    exit(2);
  };

  sockaddr_in servaddr = {0};
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = INADDR_ANY;
  servaddr.sin_port = htons(port);

  ret = bind(sockfd, (sockaddr *)&servaddr, sizeof(servaddr));
  if (ret != 0) {
    if (errno == EADDRINUSE) {
      printf( "port %d already in use\n", port );
    }
    else {
      printf( "bind error\n" );
    }
    exit(2);
  }

  ret = listen(sockfd, LISTENQ);
  if (ret != 0) {
    printf( "listen error\n" );
    exit(2);
  }
  
  pthread_mutex_init(&Nthreads_mutex, NULL);
  printf("running on port %d\n", port);

  for (;;) {
    sockaddr_in clientInfo;
    socklen_t addrlen = sizeof(clientInfo);
    SOCKET f = accept(sockfd, (sockaddr*)&clientInfo, &addrlen);
    
    if (f < 0) {
      printf("connection error\n");
    }
    else {
      std::string ip = inet_ntoa(clientInfo.sin_addr);
      printf("connection from %s:%d. socket id = %d\n", 
          ip.c_str(),
          ntohs(clientInfo.sin_port), f);
      // only accepts localhost
      if (ip != "127.0.0.1") {
        printf("access blocked\n");
        closesocket(f);
        continue;
      }
      int getThread = false;
      pthread_mutex_lock(&Nthreads_mutex);
      if (Nthreads < MAX_THREADS) {
        Nthreads += 1;
        getThread = true;
      }
      pthread_mutex_unlock(&Nthreads_mutex);
      if (getThread) {
        pthread_t tid;
        ThreadParam *param = new ThreadParam;
        param->socket = f;
        pthread_create(&tid, NULL, threadRunner, param);
        pthread_detach(tid);
        printf("connected from %s:%d tid %d\n", ip.c_str(), ntohs(clientInfo.sin_port), (int)tid);
      }
      else {
        printf("too many connections\n");
        closesocket(f);
        continue;
      }
    }
  }
  return 0;
}
