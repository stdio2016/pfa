#include <cmath>
#include <stdio.h>
#include <cstdint>
#include <algorithm>
#include <string>
#include <stdexcept>
#include <fstream>
#include <sstream>
#include <omp.h>
#include "lib/ReadAudio.hpp"
#include "lib/Timing.hpp"
#include "lib/Signal.hpp"
#include "Landmark.hpp"
#include "lib/utils.hpp"

struct match_t {
  int offset;
  int score;
};

int loadDatabase(
    std::string dir,
    std::vector<long long> &db_key,
    std::vector<uint32_t> &db_val
) {
  int key_n = 1<<24;
  db_key.resize(key_n + 1);
  std::ifstream fin(dir + "/landmarkKey.lmdb");
  if (!fin) {
    printf("cannot read landmarkKey!\n");
    return 1;
  }
  long long sum = 0;
  {
    std::vector<uint32_t> tmp(key_n);
    fin.read((char *)tmp.data(), sizeof(uint32_t) * key_n);
    for (int i = 0; i < key_n; i++) {
      sum += tmp[i];
      db_key[i+1] = sum;
    }
  }
  fin.close();
  printf("keys = %lld\n", db_key[key_n]);
  
  db_val.resize(sum);
  fin.open(dir + "/landmarkValue.lmdb");
  if (!fin) {
    printf("cannot read landmarkValue!\n");
    return 1;
  }
  long long ptr = 0;
  while (ptr < sum) {
    int maxread = std::min(sum - ptr, 10000000LL);
    fin.read((char*)(db_val.data() + ptr), maxread * sizeof(uint32_t));
    ptr += maxread;
  }
  return 0;
}

std::vector<Landmark> getLandmarks(
    std::string name,
    const std::vector<float> &dat,
    LandmarkBuilder &builder
) {
  std::vector<Peak> peaks_cum;
  std::vector<Landmark> lms_cum;
  std::vector<double> spec;
  double rms = 0.0;
  int shift = (builder.FFT_SIZE - builder.NOVERLAP) / 4;
  for (int i = 0; i < 4; i++) {
    std::vector<float> slice(dat.begin() + shift*i, dat.end());
    
    std::vector<Peak> peaks = builder.find_peaks(slice);
    if (i == 0) {
      spec = builder.spec;
      rms = builder.rms;
    }
    
    std::vector<Landmark> lms = builder.peaks_to_landmarks(peaks);
    
    peaks_cum.insert(peaks_cum.end(), peaks.begin(), peaks.end());
    lms_cum.insert(lms_cum.end(), lms.begin(), lms.end());
  }
  
  std::string bmpName = name.substr(0, name.size()-4) + "_spec.bmp";
  builder.spec = spec;
  //builder.drawSpecgram(bmpName.c_str(), peaks_cum);
  
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
    fwrite(lms_cum.data(), sizeof(Landmark), lms_cum.size(), fout);
    fclose(fout);
  }

  if (builder.log_file) {
    fprintf(builder.log_file, "compute %s rms=%.2fdB peak=%d landmarks=%d\n", shortname.c_str(),
      log10(rms) * 20, (int)peaks_cum.size(), (int)lms_cum.size());
  }
  
  return lms_cum;
}

int processQuery(
    std::string name,
    LandmarkBuilder builder,
    int queryId,
    int nSongs,
    const std::vector<long long> &db_key,
    const std::vector<uint32_t> &db_val,
    match_t *scores
) {
  Timing tm;
  try {
    Sound snd = ReadAudio(name.c_str());
    if (builder.log_file)
      fprintf(builder.log_file, "read file %.3fms\n", tm.getRunTime());

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
    if (builder.log_file)
      fprintf(builder.log_file, "stereo to mono %.3fms\n", tm.getRunTime());

    tm.getRunTime();
    channels = 1;
    double rate = (double)snd.sampleRate / (double)builder.SAMPLE_RATE;
    for (int i = 0; i < channels; i++) {
      //if (rate > 1)
      //  snd.d[i] = lopass(snd.d[i], 1.0/rate, 50);
      snd.d[i] = resample(snd.d[i], snd.sampleRate, builder.SAMPLE_RATE);
      //if (rate < 1)
      //  snd.d[i] = lopass(snd.d[i], rate, 50);
    }
    len = snd.length();
    if (builder.log_file)
      fprintf(builder.log_file, "resample %.3fms\n", tm.getRunTime());
    
    std::vector<Landmark> lms = getLandmarks(name, snd.d[0], builder);
    
    tm.getRunTime();
    std::vector<int> hist(nSongs+1);
    for (Landmark lm : lms) {
      uint32_t dt = (lm.time2 - lm.time1) & ((1<<6)-1);
      uint32_t df = (lm.freq2 - lm.freq1) & ((1<<9)-1);
      uint32_t f1 = lm.freq1 & ((1<<9)-1);
      uint32_t key = f1<<15 | df<<6 | dt;
      for (long long it = db_key[key]; it < db_key[key+1]; it++) {
        uint32_t val = db_val[it];
        uint32_t songId = val>>14;
        if (songId < nSongs) hist[songId] += 1;
      }
    }
    if (builder.log_file)
      fprintf(builder.log_file, "compute histogram %.3fms\n", tm.getRunTime());
    // counting sort
    for (int i = nSongs; i > 0; i--) hist[i] = hist[i-1];
    hist[0] = 0;
    for (int i = 0; i < nSongs; i++) hist[i+1] += hist[i];
    
    std::vector<int> matches(hist[nSongs]);
    for (Landmark lm : lms) {
      uint32_t dt = (lm.time2 - lm.time1) & ((1<<6)-1);
      uint32_t df = (lm.freq2 - lm.freq1) & ((1<<9)-1);
      uint32_t f1 = lm.freq1 & ((1<<9)-1);
      int t = lm.time1 & ((1<<14)-1);
      uint32_t key = f1<<15 | df<<6 | dt;
      
      for (long long it = db_key[key]; it < db_key[key+1]; it++) {
        uint32_t val = db_val[it];
        uint32_t songId = val>>14;
        int songT = val & ((1<<14)-1);
        if (songId < nSongs) {
          int pos = hist[songId];
          matches[pos] = songT - t;
          hist[songId] += 1;
        }
      }
    }
    if (builder.log_file)
      fprintf(builder.log_file, "counting sort %.3fms\n", tm.getRunTime());
    
    for (int i = nSongs; i > 0; i--) hist[i] = hist[i-1];
    hist[0] = 0;
    int all_song_score = 0;
    int which = 0;
    for (int i = 0; i < nSongs; i++) {
      std::sort(&matches[hist[i]], &matches[hist[i+1]]);
      int prev = 0, best_offset = 0;
      int score = 0, max_score = 0;
      for (int j = hist[i]; j < hist[i+1]; j++) {
        int t_meet = matches[j];
        if (prev == t_meet)
          score += 1;
        else
          score = 1;
        if (score > max_score) {
          max_score = score;
          best_offset = t_meet;
        }
        prev = t_meet;
      }
      scores[i].offset = best_offset;
      scores[i].score = max_score;
      if (max_score > all_song_score) {
        all_song_score = max_score;
        which = i;
      }
    }
    if (builder.log_file) {
      fprintf(builder.log_file, "sort by time %.3fms\n", tm.getRunTime());
      fprintf(builder.log_file, "best match: %d score=%d\n", which, all_song_score);
    }
    return which;
  }
  catch (std::runtime_error x) {
    printf("%s\n", x.what());
    if (builder.log_file) {
      fprintf(builder.log_file, "%s\n", x.what());
    }
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
  std::string line;
  std::vector<std::string> queryList;
  while (std::getline(flist, line)) {
    queryList.push_back(line);
  }
  flist.close();
  printf("read query list %.3fs\n", timing2.getRunTime() * 0.001);
  
  flist.open(argv[2] + std::string("/songList.txt"));
  if (!flist) {
    printf("cannot read song list!\n");
    return 1;
  }
  std::vector<std::string> songList;
  while (std::getline(flist, line)) {
    songList.push_back(line);
  }
  flist.close();
  printf("read song names %.3fs\n", timing2.getRunTime() * 0.001);
  
  int nSongs = 10000;
  std::vector<long long> db_key;
  std::vector<uint32_t> db_val;
  std::vector<int> result(queryList.size());
  std::vector<match_t> scores(queryList.size() * nSongs);
  if (loadDatabase(argv[2], db_key, db_val)) {
    printf("cannot load database\n");
    return 1;
  }
  printf("load database %.3fs\n", timing2.getRunTime() * 0.0011);
  
  LandmarkBuilder builder;
  time_t start_time;
  time(&start_time);
  char namebuf[100];
  struct tm timeinfo = *localtime(&start_time);
  strftime(namebuf, 98, "%Y%m%d-%H%M%S", &timeinfo);
  #pragma omp parallel firstprivate(builder)
  {
    std::stringstream ss;
    ss << "logs/" << "matcher" << namebuf;
    ss << "t" << omp_get_thread_num();
    ss << ".log";
    builder.log_file = fopen(ss.str().c_str(), "w");
    #pragma omp for schedule(dynamic)
    for (int i = 0; i < queryList.size(); i++) {
      std::string name = queryList[i];
      if (builder.log_file)
        fprintf(builder.log_file, "File: %s\n", name.c_str());
      fprintf(stdout, "File: %s\n", name.c_str());
      result[i] = processQuery(name, builder, i, nSongs, db_key, db_val, &scores[i*nSongs]);
      if (result[i] >= 0 && result[i] < nSongs) {
        printf("%s\t%s\n", name.c_str(), songList[result[i]].c_str());
      }
      else {
        printf("%s\t%s\n", name.c_str(), "error");
      }
    }
    if (builder.log_file) fclose(builder.log_file);
  }
  
  std::ofstream fout(argv[3]);
  if (!fout) {
    printf("cannot write result!\n");
    return 1;
  }
  for (int i = 0; i < queryList.size(); i++) {
    fout << queryList[i] << '\t';
    if (result[i] >= 0 && result[i] < nSongs)
      fout << songList[result[i]];
    else
      fout << "error";
    fout << '\n';
  }
  fout.close();
  printf("Total time: %.3fs\n", timing.getRunTime() * 0.001);
  return 0;
}
