// cl /EHsc /O2 matcher.cpp Landmark.cpp Database.cpp lib/WavReader.cpp lib/Timing.cpp lib/ReadAudio.cpp lib/BmpReader.cpp lib/Signal.cpp lib/utils.cpp lib/Sound.cpp .\PeakFinder.cpp .\PeakFinderDejavu.cpp Analyzer.cpp
#include <cmath>
#include <stdio.h>
#include <cstdint>
#include <algorithm>
#include <string>
#include <stdexcept>
#include <fstream>
#include <sstream>
#include <omp.h>
#include "lib/Timing.hpp"
#include "Landmark.hpp"
#include "lib/utils.hpp"
#include "Analyzer.hpp"
#include "Database.hpp"
#include "PeakFinderDejavu.hpp"

int processQuery(
    std::string name,
    Analyzer &analyzer,
    const Database &db,
    match_t *scores
) {
  Timing tm;
  try {
    std::vector<Landmark> lms = analyzer.fingerprint_file(name.c_str());
    
    int which = db.query_landmarks(lms, scores);
    return which;
  }
  catch (std::runtime_error x) {
    LOG_ERROR("%s", x.what());
  }
  return -1;
}

int main(int argc, char const *argv[]) {
  if (argc < 4) {
    printf("Usage: ./matcher <query list> <database dir> <result file>\n");
    return 1;
  }
  omp_set_num_threads(1);
  Timing timing, timing2;
  std::ifstream flist(argv[1]);
  if (!flist) {
    printf("cannot read query list!\n");
    return 1;
  }
  init_logger("matcher");
  std::string line;
  std::vector<std::string> queryList;
  while (std::getline(flist, line)) {
    queryList.push_back(line);
  }
  flist.close();
  LOG_DEBUG("read query list %.3fs", timing2.getRunTime() * 0.001);
  
  Database db;
  if (db.load(argv[2])) {
    LOG_FATAL("cannot load database");
    return 1;
  }
  int nSongs = db.songList.size();
  std::vector<int> result(queryList.size());
  std::vector<match_t> scores(queryList.size() * nSongs);
  LOG_DEBUG("load database %.3fs", timing2.getRunTime() * 0.0011);
  
  LandmarkBuilder builder;
  {
    Analyzer analyzer;
    analyzer.peak_finder = new PeakFinderDejavu();
    analyzer.landmark_builder = new LandmarkBuilder();
    
    for (int i = 0; i < queryList.size(); i++) {
      std::string name = queryList[i];
      LOG_DEBUG("File: %s", name.c_str());
      result[i] = processQuery(name, analyzer, db, &scores[i*nSongs]);
      if (result[i] >= 0 && result[i] < nSongs) {
        printf("%s\t%s\n", name.c_str(), db.songList[result[i]].c_str());
      }
      else {
        printf("%s\t%s\n", name.c_str(), "error");
      }
    }
    
    delete analyzer.peak_finder;
    delete analyzer.landmark_builder;
  }
  
  std::ofstream fout(argv[3]);
  if (!fout) {
    LOG_FATAL("cannot write result!");
    return 1;
  }
  for (int i = 0; i < queryList.size(); i++) {
    fout << queryList[i] << '\t';
    if (result[i] >= 0 && result[i] < nSongs)
      fout << db.songList[result[i]];
    else
      fout << "error";
    fout << '\n';
  }
  fout.close();
  
  fout.open( argv[3] + std::string(".bin"), std::ios::binary );
  if (fout) {
    fout.write((char*)(&scores[0]), sizeof(scores[0]) * scores.size());
    fout.close();
  }
  
  LOG_INFO("Total time: %.3fs", timing.getRunTime() * 0.001);
  return 0;
}
