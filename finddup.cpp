// cl /EHsc /O2 /openmp matcher.cpp Landmark.cpp Database.cpp lib/WavReader.cpp lib/Timing.cpp lib/ReadAudio.cpp lib/BmpReader.cpp lib/Signal.cpp lib/utils.cpp lib/Sound.cpp .\PeakFinder.cpp .\PeakFinderDejavu.cpp Analyzer.cpp
#include <stdio.h>
#include <cstdint>
#include <algorithm>
#include <string>
#include <stdexcept>
#include <fstream>
#include <sstream>
#include <omp.h>
#include <mpi.h>
#include "lib/Timing.hpp"
#include "Landmark.hpp"
#include "lib/utils.hpp"
#include "Database.hpp"
#include "PeakFinderDejavu.hpp"

int processQuery(
    std::string name,
    LandmarkBuilder &builder,
    const Database &db,
    int songid,
    int blacklist,
    std::ostream &fout
) {
  Timing tm;
  int oneread = 10000;
  std::vector<Peak> peaks(oneread);
  FILE *fin = fopen(name.c_str(), "rb");
  if (fin == NULL) {
    return -1;
  }
  int nread;
  int total = 0;
  while ((nread = fread(&peaks[total], sizeof(Peak), oneread, fin)) > 0) {
    total += nread;
    peaks.resize(total + oneread);
  }
  peaks.resize(total);

  int max_t = 0;
  for (Peak peak : peaks) {
    if (peak.time > max_t) max_t = peak.time;
  }

  int nsongs = db.songList.size();
  int ptr = 0;
  std::vector<match_t> scores(nsongs);
  for (int t = 0; t < max_t; t += 8000 * 10 / 512) {
    std::vector<Peak> sub_peaks;

    while (ptr < peaks.size() && peaks[ptr].time < t + 8000 * 10 / 512) {
      sub_peaks.push_back(peaks[ptr]);
      ptr++;
    }

    std::vector<Landmark> lms = builder.peaks_to_landmarks(sub_peaks);

    db.query_landmarks(lms, scores.data());

    std::vector<int> song_rank;
    for (int i = 0; i < nsongs; i++) {
      if (i != blacklist) song_rank.push_back(i);
    }

    std::sort(song_rank.begin(), song_rank.end(), [&](int a, int b){
      return scores[a].score > scores[b].score;
    });

    fout << songid << ',' << t;
    for (int rank = 0; rank < 10 && rank < song_rank.size(); rank++) {
      int which = song_rank[rank];
      fout << ',' << which << ',' << scores[which].score << ',' << scores[which].offset;
    }
    fout << '\n';
  }
  fout.flush();
  return 0;
}

int main(int argc, char *argv[]) {
  int nprocs, pid;
  MPI_Init(&argc, &argv);
  MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
  MPI_Comm_rank(MPI_COMM_WORLD, &pid);

  if (argc < 4) {
    if (pid == 0)
      printf("Usage: ./finddup <peak file list> <database dir> <result file>\n");
    return 1;
  }
  Timing timing;
  std::ifstream flist(argv[1]);
  if (!flist) {
    printf("cannot read peak list!\n");
    return 1;
  }
  char namebuf[100];
  sprintf(namebuf, "finddup-pid-%d", pid);
  init_logger(namebuf);
  std::string line;
  std::vector<std::string> queryList;
  while (std::getline(flist, line)) {
    queryList.push_back(line);
  }
  flist.close();
  LOG_DEBUG("read peak list %.3fs", timing.getRunTime() * 0.001);
  
  Database db;
  if (db.load(argv[2])) {
    LOG_FATAL("cannot load database");
    return 1;
  }

  int nSongs = db.songList.size();
  std::stringstream ss;
  ss << argv[3] << "-pid-" << pid;
  std::ofstream fout(ss.str());
  if (!fout) {
    LOG_FATAL("cannot write result!");
    return 1;
  }

  LOG_DEBUG("load database %.3fs", timing.getRunTime() * 0.001);
  
  LandmarkBuilder builder;
  for (int i = pid; i < queryList.size(); i += nprocs) {
    std::string name = queryList[i];
    LOG_INFO("File: %s", name.c_str());
    processQuery(name, builder, db, i, i, fout);
  }
  fout.close();
  
  LOG_INFO("Total time: %.3fs", timing.getRunTime() * 0.001);
  MPI_Finalize();
  return 0;
}
