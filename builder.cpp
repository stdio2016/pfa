// cl /EHsc /O2 /openmp builder.cpp Landmark.cpp lib/WavReader.cpp lib/Timing.cpp lib/ReadAudio.cpp lib/BmpReader.cpp lib/Signal.cpp lib/utils.cpp lib/Sound.cpp
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
#include "lib/ReadAudio.hpp"
#include "lib/Timing.hpp"
#include "lib/Signal.hpp"
#include "Landmark.hpp"
#include "lib/utils.hpp"

#ifdef _WIN32
#include <windows.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>

void processMusic(std::string name, LandmarkBuilder builder,
    std::vector<std::vector<uint64_t> > &db, uint32_t songId) {
  Timing tm;
  try {
    Sound snd = ReadAudio(name.c_str());
    LOG_DEBUG("read file %.3fms", tm.getRunTime());

    tm.getRunTime();
    size_t len = snd.length();
    snd.stereo_to_mono_();
    LOG_DEBUG("stereo to mono %.3fms", tm.getRunTime());

    tm.getRunTime();
    int channels = 1;
    double rate = (double)snd.sampleRate / (double)builder.SAMPLE_RATE;
    for (int i = 0; i < channels; i++) {
      //if (rate > 1)
      //  snd.d[i] = lopass(snd.d[i], 1.0/rate, 50);
      snd.d[i] = resample(snd.d[i], snd.sampleRate, builder.SAMPLE_RATE);
      //if (rate < 1)
      //  snd.d[i] = lopass(snd.d[i], rate, 50);
    }
    LOG_DEBUG("resample %.3fms", tm.getRunTime());
    
    std::vector<Peak> peaks = builder.find_peaks(snd.d[0]);
    
    tm.getRunTime();
    std::vector<Landmark> lms = builder.peaks_to_landmarks(peaks);
    LOG_DEBUG("create landmark pairs %.3fms", tm.getRunTime());
    
    std::string bmpName = name.substr(0, name.size()-4) + "_spec.bmp";
    //builder.drawSpecgram(bmpName.c_str(), peaks);
    
    std::string shortname = name;
    if (shortname.find('/') != shortname.npos) {
      shortname = shortname.substr(shortname.find_last_of('/')+1, -1);
    }
    std::string lm_file = "lm/" + shortname + ".lm";
#ifdef _WIN32
    wchar_t *lm_file_w = utf8_to_wchar(lm_file.c_str());
    FILE *fout = _wfopen(lm_file_w, L"wb");
    delete[] lm_file_w;
#else
    FILE *fout = fopen(lm_file.c_str(), "wb");
#endif
    if (fout) {
      fwrite(lms.data(), sizeof(Landmark), lms.size(), fout);
      fclose(fout);
    }
    
    tm.getRunTime();
    for (Landmark lm : lms) {
      uint32_t dt = (lm.time2 - lm.time1) & ((1<<6)-1);
      uint32_t df = (lm.freq2 - lm.freq1) & ((1<<9)-1);
      uint32_t f1 = lm.freq1 & ((1<<9)-1);
      uint32_t t = lm.time1 & ((1<<14)-1);
      uint64_t key = f1<<15 | df<<6 | dt;
      uint64_t value = songId<<14 | t;
      
      db[f1].push_back(key<<32 | value);
    }
    LOG_DEBUG("add landmark to database %.3fms", tm.getRunTime());
    
    LOG_DEBUG("compute %s duration=%.3fs rms=%.2fdB peak=%d landmarks=%d", shortname.c_str(),
        (double)len / snd.sampleRate,
        log10(builder.rms) * 20, (int)peaks.size(), (int)lms.size());
  }
  catch (std::runtime_error x) {
    printf("%s\n", x.what());
    LOG_DEBUG("%s", x.what());
  }
}

void createDirIfNotExist(const char *name) {
  struct stat st = {0};
  if (stat(name, &st) == -1) {
    #ifdef _WIN32
    CreateDirectory(name, NULL);
    #else
    mkdir(name, 0755);
    #endif
  }
}

void dumpPartialDB(
    std::vector<std::vector<uint64_t> > &db,
    int threadid,
    int dumpCount,
    const char *db_location)
{
  for (int j = 0; j < db.size(); j++) {
    std::sort(db[j].begin(), db[j].end());
  }
  std::stringstream ss;
  ss << db_location << "/tmp_" << threadid << "_" << dumpCount << ".lms";
  std::ofstream lm_out(ss.str().c_str(), std::ios::binary);
  if (lm_out) {
    for (int j = 0; j < db.size(); j++)
      lm_out.write((const char *)db[j].data(), db[j].size() * sizeof(uint64_t));
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
    printf("Usage: ./builder <music list file> <db location>\n");
    return 1;
  }
  const char *db_location = argv[2];
  std::ifstream flist(argv[1]);
  if (!flist) {
    printf("cannot read music list!\n");
    return 1;
  }
  createDirIfNotExist(db_location);
  std::ofstream flistOut(db_location + std::string("/songList.txt"));
  if (!flistOut) {
    printf("cannot write music list!\n");
    return 1;
  }
  createDirIfNotExist("lm");
  createDirIfNotExist("logs");
  std::string line;
  std::vector<std::string> filenames;
  while (std::getline(flist, line)) {
    filenames.push_back(line);
    flistOut << line << '\n';
  }
  flist.close();
  flistOut.close();
  printf("list contains %d songs\n", (int)filenames.size());
  
  Timing timing, timing2;
  LandmarkBuilder builder;
  
  init_logger("builder");
  
  int nthreads = omp_get_max_threads();
  std::vector<int> dumpCount(nthreads);
  
  printf("computing landmarks...\n");
  #pragma omp parallel firstprivate(builder)
  {
    int tid = omp_get_thread_num();
    
    std::vector<std::vector<uint64_t> > db(512);
    
    #pragma omp for schedule(dynamic)
    for (int i = 0; i < filenames.size(); i++) {
      std::string name = filenames[i];
      LOG_DEBUG("File: %s", name.c_str());
      fprintf(stdout, "File: %s\n", name.c_str());
      processMusic(name, builder, db, i);
      long long nentries = 0;
      for (int j = 0; j < db.size(); j++) {
        nentries += db[j].size();
      }
      long long maxentries = 50000 * 1000; // 50M entries ~ 400MB
      if (nentries > maxentries) {
        Timing tt;
        dumpCount[tid] += 1;
        dumpPartialDB(db, tid, dumpCount[tid], db_location);
        for (int j = 0; j < db.size(); j++) db[j].clear();
        LOG_DEBUG("sort keys %.3fms", tt.getRunTime());
      }
    }
    Timing tt;
    dumpCount[tid] += 1;
    dumpPartialDB(db, tid, dumpCount[tid], db_location);
    LOG_DEBUG("sort keys %.3fms", tt.getRunTime());
  }
  printf("compute landmark time: %.3fs\n", timing.getRunTime() * 0.001);
  
  printf("merge landmark files...\n");
  std::vector<std::string> tmp_lms;
  for (int i = 0; i < dumpCount.size(); i++) {
    for (int j = 1; j <= dumpCount[i]; j++) {
      std::stringstream ss;
      ss << db_location << "/tmp_" << i << "_" << j << ".lms";
      tmp_lms.push_back(ss.str());
    }
  }
  if (merge_db(tmp_lms, db_location + std::string("/landmarks.lms"))) {
    printf("merge failed!\n");
    return 1;
  }
  printf("merge landmark files time: %.3fs\n", timing.getRunTime() * 0.001);
  
  printf("compressing database...\n");
  if (compressDatabase(
    db_location + std::string("/landmarks.lms"),
    db_location + std::string("/landmarkKey.lmdb"),
    db_location + std::string("/landmarkValue.lmdb")
  )) {
    printf("compress failed!\n");
    return 1;
  }
  printf("compress database time: %.3fs\n", timing.getRunTime() * 0.001);
  
  printf("Total time: %.3fs\n", timing2.getRunTime() * 0.001);
  return 0;
}
