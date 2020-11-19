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
#include <cmath>
#include <pthread.h>
#include <vector>
#include <sstream>
#include <algorithm>
#include "lib/Timing.hpp"
#include "lib/ReadAudio.hpp"
#include "lib/Pitch.hpp"
#include "PitchDatabase.hpp"

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
  PitchDatabase *db;
};

int processQuery(
    std::string name,
    PitchTracking builder,
    const PitchDatabase &db,
    double *scores,
    std::vector<int> &midi
) {
  Timing tm;
  try {
    Sound snd = ReadAudio(name.c_str());

    tm.getRunTime();
    size_t len = snd.length();
    int channels = snd.numberOfChannels();
    for (int i = 1; i < channels; i++) {
      for (int j = 0; j < len; j++)
        snd.d[0][j] += snd.d[i][j];
    }
    for (int i = 0; i < len; i++) {
      snd.d[0][i] *= 1.0 / channels;
    }
    snd.d.resize(1);

    std::vector<double> pitch = getPitch(snd.d[0], snd.sampleRate, builder);
    
    midi.clear();
    int prev = 0, empty = 0;
    for (int i = 0; i < pitch.size(); i++) {
      int ptc = prev;
      if (pitch[i] != 0) {
        double trans = round(log2(pitch[i] / 440.0) * 12.0 + 69.0);
        if (trans >= 36 && trans <= 77) { ptc = trans; empty = 0; }
        else empty++;
      }
      else empty++;
      if (ptc && empty < 30)
        midi.push_back(ptc);
      prev = ptc;
    }
    
    db.query_pitch(midi, scores, NULL);
    return 0;
  }
  catch (std::runtime_error x) {
    printf("%s\n", x.what());
  }
  return -1;
}

void runCommand(std::string line, SOCKET socket, ThreadParam *param) {
  std::stringstream ss;
  std::string cmd, file;
  ss << line;
  ss >> cmd >> file;
  cmd = onlyAscii(cmd);
  file = onlyAscii(file);
  printf("command: %s %s\n", cmd.c_str(), file.c_str());
  if (cmd == "query") {
    PitchTracking builder;
    const PitchDatabase &db = *param->db;
    int nSongs = db.songList.size();
    std::vector<double> scores(nSongs);
    std::stringstream outs;
    std::vector<int> midi;
    int ret = processQuery(
        file,
        builder,
        db,
        scores.data(),
        midi
    );
    if (ret == -1) {
      outs << "{\"progress\":\"error\",\"reason\":\"file not found\"}\r\n";
    }
    else {
      std::vector<int> ind(nSongs);
      for (int i  = 0; i < nSongs; i++) ind[i] = i;
      std::sort(ind.begin(), ind.end(), [scores](int a, int b){
        return scores[a] > scores[b];
      });
      outs << "{\"progress\":100,\"songs\":[";
      for (int i = 0; i < 10 && i < nSongs; i++) {
        if (i) outs << ",";
        int me = ind[i];
        std::string shortname = db.songList[me];
        if (shortname.find('/') != shortname.npos) {
          shortname = shortname.substr(shortname.find_last_of('/')+1, -1);
        }
        outs << "{\"name\":\"" << shortname << "\",";
        outs << "\"file\":\"" << db.srcList[me] << "\",";
        outs << "\"score\":" << scores[me] << "}";
      }
      outs << "],\"pitch\":[";
      for (int i = 0; i < midi.size(); i++) {
        if (i) outs<<',';
        outs << midi[i];
      }
      outs << "]}\r\n";
    }
    std::string out = outs.str();
    send(socket, out.c_str(), out.size(), 0);
  }
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
        runCommand(linebuf.str(), socket, param);
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
  
  if (argc < 2) {
    return 1;
  }

  PitchDatabase db;
  if (db.load(argv[1])) {
    printf("Failed to load PitchDatabase\n");
    return 1;
  }

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
        param->db = &db;
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
