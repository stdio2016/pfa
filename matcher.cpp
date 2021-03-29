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
#include "Database.hpp"

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
    const Database &db,
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
    
    int which = db.query_landmarks(lms, scores, builder.log_file);
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
  
  Database db;
  if (db.load(argv[2])) {
    printf("cannot load database\n");
    return 1;
  }
  int nSongs = db.songList.size();
  std::vector<int> result(queryList.size());
  std::vector<match_t> scores(queryList.size() * nSongs);
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
      result[i] = processQuery(name, builder, db, &scores[i*nSongs]);
      if (result[i] >= 0 && result[i] < nSongs) {
        printf("%s\t%s\n", name.c_str(), db.songList[result[i]].c_str());
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
  
  printf("Total time: %.3fs\n", timing.getRunTime() * 0.001);
  return 0;
}
