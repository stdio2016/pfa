// cl /EHsc /O2 /openmp builder.cpp Landmark.cpp Database.cpp lib/WavReader.cpp lib/Timing.cpp lib/ReadAudio.cpp lib/BmpReader.cpp lib/Signal.cpp lib/utils.cpp lib/Sound.cpp PeakFinder.cpp PeakFinderDejavu.cpp Analyzer.cpp
#include <cmath>
#include <stdio.h>
#include <ctime>
#include <cstdint>
#include <omp.h>
#include <algorithm>
#include <string>
#include <cstring>
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
    tm.getRunTime();
    
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
  uint32_t key_count = 1<<24;
  std::sort(db.begin(), db.end());
  std::stringstream ss;
  ss << db_location << "/tmp_" << threadid << "_" << dumpCount << "Key";
  std::ofstream lm_outKey(ss.str().c_str(), std::ios::binary);
  
  ss = std::stringstream();
  ss << db_location << "/tmp_" << threadid << "_" << dumpCount << "Value";
  std::ofstream lm_outVal(ss.str().c_str(), std::ios::binary);
  if (lm_outKey && lm_outVal) {
    std::vector<uint32_t> cnt(key_count, 0), values;
    values.reserve(db.size());
    for (uint64_t x : db) {
      uint32_t key = x>>32;
      if (key < key_count) cnt[key] += 1;
      values.push_back(x);
    }
    lm_outVal.write((const char *)values.data(), values.size() * sizeof(uint32_t));
    // choose the best encoding to save space
    if (db.size() >= key_count) {
      lm_outKey.write("dens", 4);
      lm_outKey.write((const char *)cnt.data(), cnt.size() * sizeof(uint32_t));
    }
    else {
      values.clear();
      for (uint64_t x : db) {
        uint32_t key = x>>32;
        values.push_back(key);
      }
      lm_outKey.write("keys", 4);
      lm_outKey.write((const char *)values.data(), values.size() * sizeof(uint32_t));
    }
  }
  else {
    LOG_ERROR("cannot write temporary database");
  }
}

int merge_db(std::vector<std::string> filenames, std::string out_file) {
  int key_count = 1<<24;
  int bufsize = 20000000;
  int nfiles = filenames.size();
  std::string out_key = out_file + "Key";
  FILE *fout = fopen(out_key.c_str(), "wb");
  if (!fout) {
    LOG_FATAL("cannot open file %s", out_file.c_str());
    return 1;
  }
  std::vector<std::vector<uint32_t> > sub_cnt;
  std::vector<uint32_t> tot_cnt(key_count, 0);
  for (int i = 0; i < nfiles; i++) {
    std::string key_file = filenames[i] + "Key";
    FILE *fin = fopen(key_file.c_str(), "rb");
    if (!fin) {
      LOG_FATAL("cannot open temporary file %s", key_file.c_str());
      return 1;
    }
    sub_cnt.push_back(std::vector<uint32_t>(key_count, 0));
    char buf[4] = {0};
    fread(buf, 4, 1, fin);
    if (memcmp(buf, "keys", 4) == 0) {
      std::vector<uint32_t> buf2(1000);
      int nread = 0;
      while ((nread = fread((char *)buf2.data(), sizeof(uint32_t), 1000, fin)) > 0) {
        for (int j = 0; j < nread; j++) {
          sub_cnt[i][buf2[j]] += 1;
        }
      }
    }
    else if (memcmp(buf, "dens", 4) == 0) {
      fread((char *)sub_cnt[i].data(), sizeof(uint32_t), key_count, fin);
    }
    fclose(fin);
    for (int j = 0; j < key_count; j++) {
      tot_cnt[j] += sub_cnt[i][j];
    }
  }
  fwrite((char *)tot_cnt.data(), sizeof(uint32_t), key_count, fout);
  fclose(fout);
  
  out_key = out_file + "Value";
  fout = fopen(out_key.c_str(), "wb");
  std::vector<FILE *> files;
  for (int i = 0; i < nfiles; i++) {
    std::string value_file = filenames[i] + "Value";
    FILE *fin = fopen(value_file.c_str(), "rb");
    if (!fin) {
      LOG_FATAL("cannot open temporary file %s", value_file.c_str());
      return 1;
    }
    files.push_back(fin);
  }
  int current = 0, prev = 0;
  size_t amount = 0;
  while (current < key_count) {
    amount += tot_cnt[current];
    current += 1;
    if (amount > bufsize || current == key_count) {
      std::vector<std::vector<uint32_t> > bufs;
      for (int i = 0; i < nfiles; i++) {
        size_t need = 0;
        for (int j = prev; j < current; j++) need += sub_cnt[i][j];
        bufs.push_back(std::vector<uint32_t>(need));
        fread((char *)bufs[i].data(), sizeof(uint32_t), need, files[i]);
      }
      std::vector<size_t> pos(nfiles);
      for (int j = prev; j < current; j++) {
        for (int i = 0; i < nfiles; i++) {
          fwrite((char *)&bufs[i][pos[i]], sizeof(uint32_t), sub_cnt[i][j], fout);
          pos[i] += sub_cnt[i][j];
        }
      }
      amount = 0;
      prev = current;
    }
  }
  for (FILE *fin : files) {
    fclose(fin);
  }
  fclose(fout);
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
  Database db_config;
  
  int nthreads = omp_get_max_threads();
  std::vector<int> dumpCount(nthreads);
  
  #pragma omp parallel
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
      long long maxentries = 20000 * 1000; // 20M entries ~ 160MB
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
      ss << db_location << "/tmp_" << i << "_" << j;
      tmp_lms.push_back(ss.str());
    }
  }
  if (merge_db(tmp_lms, db_location + std::string("/landmark"))) {
    LOG_FATAL("merge failed!");
    return 1;
  }
  LOG_INFO("merge landmark files time: %.3fs", timing.getRunTime() * 0.001);
  
  LOG_INFO("Total time: %.3fs", timing2.getRunTime() * 0.001);
  return 0;
}
