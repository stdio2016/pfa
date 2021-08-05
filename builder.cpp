// cl /EHsc /O2 /openmp builder.cpp Landmark.cpp Database.cpp lib/WavReader.cpp lib/Timing.cpp lib/ReadAudio.cpp lib/BmpReader.cpp lib/Signal.cpp lib/utils.cpp lib/Sound.cpp PeakFinder.cpp PeakFinderDejavu.cpp Analyzer.cpp
#include <cmath>
#include <stdio.h>
#include <ctime>
#include <cstdint>
#include <omp.h>
#include <algorithm>
#include <string>
#include <stdexcept>
#include <fstream>
#include <sstream>
#include <queue>
#include <functional>
#include "lib/Timing.hpp"
#include "Landmark.hpp"
#include "lib/utils.hpp"
#include "Analyzer.hpp"
#include "PeakFinderDejavu.hpp"
#include "Database.hpp"

#ifdef _WIN32
#include <windows.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>

void processMusic(std::string name, Analyzer &analyzer,
    const Database &db_config,
    std::vector<uint64_t> &tmp_db, uint32_t songId) {
  Timing tm;
  try {
    std::vector<Landmark> lms = analyzer.fingerprint_file(name.c_str());
    LOG_DEBUG("create landmark pairs %.3fms", tm.getRunTime());
    
    std::string shortname = name;
    if (shortname.find('/') != shortname.npos) {
      shortname = shortname.substr(shortname.find_last_of('/')+1, -1);
    }
    
    std::vector<uint32_t> hash(lms.size() * 2);
    db_config.landmark_to_hash(lms.data(), lms.size(), songId, hash.data());
    for (size_t i = 0; i < lms.size(); i++) {
      uint64_t key = hash[i*2];
      uint64_t value = hash[i*2+1];
      tmp_db.push_back(key<<32 | value);
    }
    LOG_DEBUG("add landmark to database %.3fms", tm.getRunTime());
  }
  catch (std::runtime_error x) {
    LOG_ERROR("%s", x.what());
  }
}

void createDirIfNotExist(const char *name) {
  struct stat st = {0};
  if (stat(name, &st) == -1) {
    #ifdef _WIN32
    int status = CreateDirectory(name, NULL);
    if (status == 0) {
      LOG_ERROR("Cannot create directory %s", name);
    }
    #else
    int status = mkdir(name, 0755);
    if (status != 0) {
      LOG_ERROR("Cannot create directory %s", name);
    }
    #endif
  }
}

void dumpPartialDB(
    std::vector<uint64_t> &db,
    int threadid,
    int dumpCount,
    const char *db_location)
{
  std::sort(db.begin(), db.end());
  std::stringstream ss;
  ss << db_location << "/tmp_" << threadid << "_" << dumpCount << ".lms";
  std::ofstream lm_out(ss.str().c_str(), std::ios::binary);
  if (lm_out) {
    lm_out.write((const char *)db.data(), db.size() * sizeof(uint64_t));
  }
}

int merge_db(std::vector<std::string> filenames, std::string out_file) {
  int bufsize = 2000000;
  int nfiles = filenames.size();
  FILE *fout = fopen(out_file.c_str(), "wb");
  if (!fout) {
    fprintf(stderr, "cannot open file %s\n", out_file.c_str());
    return 1;
  }
  std::vector<FILE*> files(nfiles);
  std::vector<uint64_t> buf(nfiles * bufsize);
  std::vector<int> pos(nfiles);
  std::vector<int> total(nfiles);
  typedef std::pair<uint64_t, int> typep;
  std::priority_queue<typep, std::vector<typep>, std::greater<typep>> pq;
  for (int i = 0; i < nfiles; i++) {
    files[i] = fopen(filenames[i].c_str(), "rb");
    if (!files[i]) {
      fprintf(stderr, "cannot open file %s\n", filenames[i].c_str());
      return 1;
    }
    total[i] = fread(buf.data()+i*bufsize, sizeof(uint64_t), bufsize, files[i]);
    if (total[i] > 0) {
      pq.push(typep(buf[i*bufsize], i));
      pos[i] += 1;
    }
  }
  
  std::vector<uint64_t> out;
  while (!pq.empty()) {
    typep cho = pq.top();
    pq.pop();
    out.push_back(cho.first);
    if (out.size() == bufsize) {
      fwrite(out.data(), sizeof(uint64_t), bufsize, fout);
      out.clear();
    }
    int id = cho.second;
    if (pos[id] == total[id] && total[id] == bufsize) {
      // still has data
      total[id] = fread(buf.data()+id*bufsize, sizeof(uint64_t), bufsize, files[id]);
      pos[id] = 0;
      if (total[id] > 0) {
        pq.push(typep(buf[id*bufsize], id));
        pos[id] += 1;
      }
    }
    else if (pos[id] < total[id]) {
      pq.push(typep(buf[id*bufsize + pos[id]], id));
      pos[id] += 1;
    }
  }
  fwrite(out.data(), sizeof(uint64_t), out.size(), fout);
  fclose(fout);
  for (int i = 0; i < nfiles; i++) {
    fclose(files[i]);
  }
  return 0;
}

int compressDatabase(
    std::string lms,
    std::string keyOut,
    std::string valOut
) {
  FILE *fin = fopen(lms.c_str(), "rb");
  if (!fin) {
    fprintf(stderr, "cannot open file %s\n", lms.c_str());
    return 1;
  }
  FILE *fkey = fopen(keyOut.c_str(), "wb");
  if (!fkey) {
    fprintf(stderr, "cannot open file %s\n", keyOut.c_str());
    return 1;
  }
  FILE *fval = fopen(valOut.c_str(), "wb");
  if (!fval) {
    fprintf(stderr, "cannot open file %s\n", valOut.c_str());
    return 1;
  }
  int buf_size = 1000000;
  int key_count = 1<<24;
  std::vector<uint64_t> buf(buf_size);
  std::vector<uint32_t> buf2(buf_size);
  std::vector<uint32_t> keyDat(key_count);
  int nread = 0;
  while ((nread = fread(buf.data(), sizeof(uint64_t), buf_size, fin)) > 0) {
    for (int i = 0; i < nread; i++) {
      uint32_t key = buf[i]>>32;
      buf2[i] = buf[i];
      keyDat[key] += 1;
    }
    fwrite(buf2.data(), sizeof(uint32_t), nread, fval);
  }
  fclose(fin);
  fclose(fval);
  fwrite(keyDat.data(), sizeof(uint32_t), key_count, fkey);
  fclose(fkey);
  return 0;
}

int main(int argc, char const *argv[]) {
  if (argc < 3) {
    fprintf(stderr, "Usage: ./builder <music list file> <db location>\n");
    return 1;
  }
  const char *db_location = argv[2];
  std::ifstream flist(argv[1]);
  if (!flist) {
    fprintf(stderr, "cannot read music list!\n");
    return 1;
  }
  createDirIfNotExist(db_location);
  std::ofstream flistOut(db_location + std::string("/songList.txt"));
  if (!flistOut) {
    fprintf(stderr, "cannot write music list!\n");
    return 1;
  }
  createDirIfNotExist("lm");
  createDirIfNotExist("logs");
  init_logger("builder");
  std::string line;
  std::vector<std::string> filenames;
  while (std::getline(flist, line)) {
    filenames.push_back(line);
    flistOut << line << '\n';
  }
  flist.close();
  flistOut.close();
  LOG_DEBUG("list contains %d songs", (int)filenames.size());
  
  Timing timing, timing2;
  LandmarkBuilder builder;
  Database db_config;
  
  int nthreads = omp_get_max_threads();
  std::vector<int> dumpCount(nthreads);
  
  #pragma omp parallel firstprivate(builder)
  {
    int tid = omp_get_thread_num();
    
    Analyzer analyzer;
    analyzer.peak_finder = new PeakFinderDejavu();
    analyzer.landmark_builder = new LandmarkBuilder();
    
    std::vector<uint64_t> db;
    
    #pragma omp for schedule(dynamic)
    for (int i = 0; i < filenames.size(); i++) {
      std::string name = filenames[i];
      LOG_INFO("File: %s", name.c_str());
      processMusic(name, analyzer, db_config, db, i);
      long long nentries = db.size();
      long long maxentries = 50000 * 1000; // 50M entries ~ 400MB
      if (nentries > maxentries) {
        Timing tt;
        dumpCount[tid] += 1;
        dumpPartialDB(db, tid, dumpCount[tid], db_location);
        db.clear();
        LOG_DEBUG("sort keys %.3fms", tt.getRunTime());
      }
    }
    Timing tt;
    dumpCount[tid] += 1;
    dumpPartialDB(db, tid, dumpCount[tid], db_location);
    LOG_DEBUG("sort keys %.3fms", tt.getRunTime());
    delete analyzer.landmark_builder;
    delete analyzer.peak_finder;
  }
  LOG_INFO("compute landmark time: %.3fs", timing.getRunTime() * 0.001);
  
  std::vector<std::string> tmp_lms;
  for (int i = 0; i < dumpCount.size(); i++) {
    for (int j = 1; j <= dumpCount[i]; j++) {
      std::stringstream ss;
      ss << db_location << "/tmp_" << i << "_" << j << ".lms";
      tmp_lms.push_back(ss.str());
    }
  }
  if (merge_db(tmp_lms, db_location + std::string("/landmarks.lms"))) {
    LOG_FATAL("merge failed!");
    return 1;
  }
  LOG_INFO("merge landmark files time: %.3fs", timing.getRunTime() * 0.001);
  
  if (compressDatabase(
    db_location + std::string("/landmarks.lms"),
    db_location + std::string("/landmarkKey.lmdb"),
    db_location + std::string("/landmarkValue.lmdb")
  )) {
    LOG_FATAL("compress failed!");
    return 1;
  }
  LOG_INFO("compress database time: %.3fs", timing.getRunTime() * 0.001);
  
  LOG_INFO("Total time: %.3fs", timing2.getRunTime() * 0.001);
  return 0;
}
