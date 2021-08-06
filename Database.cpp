#include <algorithm>
#include <fstream>
#include <omp.h>
#include <string>
#include "Database.hpp"
#include "lib/Timing.hpp"
#include "lib/utils.hpp"

int Database::load(std::string dir) {
  std::ifstream flist;
  flist.open(dir + std::string("/songList.txt"));
  if (!flist) {
    LOG_FATAL("cannot read song list!");
    return 1;
  }
  songList.clear();
  std::string line;
  while (std::getline(flist, line)) {
    songList.push_back(line);
  }
  flist.close();
  LOG_DEBUG("nsongs %d", (int)songList.size());
  
  int key_n = 1<<24;
  db_key.resize(key_n + 1);
  std::ifstream fin(dir + "/landmarkKey", std::ifstream::binary);
  if (!fin) {
    LOG_FATAL("cannot read landmarkKey!");
    return 1;
  }
  uint64_t sum = 0;
  {
    std::vector<uint32_t> tmp(key_n);
    fin.read((char *)tmp.data(), sizeof(uint32_t) * key_n);
    for (int i = 0; i < key_n; i++) {
      sum += tmp[i];
      db_key[i+1] = sum;
    }
  }
  fin.close();
  LOG_DEBUG("keys = %lld", (long long) db_key[key_n]);
  
  db_val.resize(sum);
  fin.open(dir + "/landmarkValue", std::ifstream::binary);
  if (!fin) {
    LOG_FATAL("cannot read landmarkValue!");
    return 1;
  }
  uint64_t ptr = 0;
  while (ptr < sum) {
    int maxread = std::min<uint64_t>(sum - ptr, 10000000ULL);
    fin.read((char*)(db_val.data() + ptr), maxread * sizeof(uint32_t));
    ptr += maxread;
  }
  return 0;
}

int Database::query_landmarks(
    const std::vector<Landmark> &lms,
    match_t *out_scores
) const {
  int nSongs = songList.size();
  Timing tm;
  std::vector<int> hist(nSongs+1);
  std::vector<uint32_t> hash(lms.size() * 2);
  landmark_to_hash(lms.data(), lms.size(), 0, hash.data());
  const int T1_BITS = this->T1_BITS;

  const int nthreads = omp_get_max_threads();
  std::vector<int> hist_th(nthreads * nSongs);
  #pragma omp parallel for schedule(static,1)
  for (int i = 0; i < lms.size(); i++) {
    const int tid = omp_get_thread_num();
    int *hist_local = &hist_th[tid * nSongs];
    uint32_t key = hash[i*2];
    for (uint64_t it = db_key[key]; it < db_key[key+1]; it++) {
      uint32_t val = db_val[it];
      uint32_t songId = val>>T1_BITS;
      if (songId < nSongs) hist_local[songId] += 1;
    }
  }
  for (int i = 0; i < nthreads; i++)
    for (int j = 0; j < nSongs; j++)
      hist[j] += hist_th[i * nSongs + j];
  LOG_DEBUG("compute histogram %.3fms", tm.getRunTime());
  // counting sort
  for (int i = nSongs; i > 0; i--) hist[i] = hist[i-1];
  hist[0] = 0;
  for (int i = 0; i < nSongs; i++) hist[i+1] += hist[i];

  for (int i = 0; i < nSongs; i++) {
    int prev = hist_th[i] + hist[i];
    hist_th[i] = hist[i];
    for (int j = 1; j < nthreads; j++) {
      int t = hist_th[j * nSongs + i];
      hist_th[j * nSongs + i] = prev;
      prev = t + prev;
    }
  }
  
  std::vector<int> matches(hist[nSongs]);
  #pragma omp parallel for schedule(static,1)
  for (int i = 0; i < lms.size(); i++) {
    const int tid = omp_get_thread_num();
    int *hist_local = &hist_th[tid * nSongs];
    int t = hash[i*2+1];
    uint32_t key = hash[i*2];
    
    for (uint64_t it = db_key[key]; it < db_key[key+1]; it++) {
      uint32_t val = db_val[it];
      uint32_t songId = val>>T1_BITS;
      int songT = val & ((1<<T1_BITS)-1);
      if (songId < nSongs) {
        int pos = hist_local[songId];
        matches[pos] = songT - t;
        hist_local[songId] += 1;
      }
    }
  }
  LOG_DEBUG("counting sort %.3fms items %d", tm.getRunTime(), hist[nSongs]);
  
  //for (int i = nSongs; i > 0; i--) hist[i] = hist[i-1];
  hist[0] = 0;
  int all_song_score = 0;
  int which = 0;
  #pragma omp parallel
  {
  int my_song_score = 0;
  int my_which = 0;
  #pragma omp for schedule(static,16)
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
    if (out_scores) {
      out_scores[i].offset = best_offset;
      out_scores[i].score = max_score;
    }
    if (max_score > my_song_score) {
      my_song_score = max_score;
      my_which = i;
    }
  }
  #pragma omp critical
  if (my_song_score > all_song_score) {
    all_song_score = my_song_score;
    which = my_which;
  }
  }
  LOG_DEBUG("sort by time %.3fms", tm.getRunTime());
  LOG_DEBUG("best match: %d score=%d", which, all_song_score);
  return which;
}

void Database::landmark_to_hash(const Landmark *lms, size_t len, uint32_t song_id, uint32_t *hash_out) const {
  const int T1_BITS = this->T1_BITS;
  const int F1_BITS = this->F1_BITS;
  const int DT_BITS = this->DT_BITS;
  const int DF_BITS = this->DF_BITS;
  for (size_t i = 0; i < len; i++) {
    Landmark lm = lms[i];
    uint32_t f1 = lm.freq1 & ((1<<F1_BITS)-1);
    uint32_t df = (lm.freq2 - lm.freq1) & ((1<<DF_BITS)-1);
    uint32_t dt = (lm.time2 - lm.time1) & ((1<<DT_BITS)-1);
    uint32_t key = f1<<(DF_BITS+DT_BITS) | df<<DT_BITS | dt;
    
    uint32_t t1 = lm.time1 & ((1<<T1_BITS)-1);
    uint32_t value = song_id<<T1_BITS | t1;
    hash_out[i*2] = key;
    hash_out[i*2+1] = value;
  }
}
