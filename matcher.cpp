// cl /EHsc /O2 /openmp matcher.cpp Landmark.cpp Database.cpp lib/WavReader.cpp lib/Timing.cpp lib/ReadAudio.cpp lib/BmpReader.cpp lib/Signal.cpp lib/utils.cpp lib/Sound.cpp .\PeakFinder.cpp .\PeakFinderDejavu.cpp Analyzer.cpp
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
  Timing timing;
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
  LOG_DEBUG("read query list %.3fs", timing.getRunTime() * 0.001);
  
  Database db;
  if (db.load(argv[2])) {
    LOG_FATAL("cannot load database");
    return 1;
  }

  int nSongs = db.songList.size();
  std::ofstream fout(argv[3]);
  if (!fout) {
    LOG_FATAL("cannot write result!");
    return 1;
  }

  std::ofstream fout_bin( argv[3] + std::string(".bin"), std::ios::binary );
  if (!fout_bin) {
    LOG_FATAL("cannot write result!");
    return 1;
  }

  std::vector<int> result(queryList.size());
  std::vector<match_t> scores(nSongs);
  LOG_DEBUG("load database %.3fs", timing.getRunTime() * 0.001);
  
  LandmarkBuilder builder;
  {
    Analyzer analyzer;
    analyzer.REPEAT_COUNT = 4;
    analyzer.peak_finder = new PeakFinderDejavu();
    analyzer.landmark_builder = new LandmarkBuilder();
    
    for (int i = 0; i < queryList.size(); i++) {
      std::string name = queryList[i];
      LOG_DEBUG("File: %s", name.c_str());
      result[i] = processQuery(name, analyzer, db, scores.data());
      if (result[i] >= 0 && result[i] < nSongs) {
        int score = scores[result[i]].score;
        int hop = analyzer.peak_finder->FFT_SIZE - analyzer.peak_finder->NOVERLAP;
        double match_time = double(scores[result[i]].offset) * hop / analyzer.SAMPLE_RATE;
        LOG_INFO("%s\t%s\tscore=%d\ttime=%.2f", name.c_str(), db.songList[result[i]].c_str(), score, match_time);
        fout << queryList[i] << '\t' << db.songList[result[i]] << '\n';
      }
      else {
        LOG_ERROR("error querying %s", name.c_str());
        fout << queryList[i] << '\t' << "error\n";
      }
      fout.flush();
      fout_bin.write((char*)scores.data(), sizeof(scores[0]) * scores.size());
      fout_bin.flush();
    }
    
    delete analyzer.peak_finder;
    delete analyzer.landmark_builder;
  }
  fout.close();
  fout_bin.close();
  
  LOG_INFO("Total time: %.3fs", timing.getRunTime() * 0.001);
  return 0;
}
