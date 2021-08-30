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
  
  db_val = new uint32_t[sum];
  fin.open(dir + "/landmarkValue", std::ifstream::binary);
  if (!fin) {
    LOG_FATAL("cannot read landmarkValue!");
    return 1;
  }
  uint64_t ptr = 0;
  while (ptr < sum) {
    int maxread = std::min<uint64_t>(sum - ptr, 10000000ULL);
    fin.read((char*)(db_val + ptr), maxread * sizeof(uint32_t));
    ptr += maxread;
  }
  return 0;
}

static int score_song_sorted(const uint32_t *matches, size_t n, int nsongs, match_t *scores) {
  #pragma omp parallel
  {
    // initialize
    #pragma omp for
    for (int i = 0; i < nsongs; i++) {
      scores[i].score = 0;
      scores[i].offset = 0;
    }

    // split task
    int nthreads = omp_get_num_threads();
    int tid = omp_get_thread_num();
    size_t step = n / nthreads;
    size_t start = step * tid;
    start += std::min<size_t>(tid, n % nthreads);
    size_t end = step * (tid+1);
    end += std::min<size_t>(tid+1, n % nthreads);
    // move to next song start
    if (start != 0) {
      while (start < n && matches[start]>>14 == matches[start-1]>>14) {
        start++;
      }
    }
    for (size_t i = start; i < end; i++) {
      int songId = matches[i]>>14;
      int cur = 0, prev = 0;
      int best = 0, best_t = 0;
      while (i < n && matches[i]>>14 == songId) {
        int t = matches[i] & ((1<<14)-1);
        if (matches[i] == prev) cur += 1;
        else cur = 1;
        if (cur > best) {
          best = cur;
          best_t = t;
        }
        prev = matches[i];
        i += 1;
      }
      if (songId < nsongs) {
        scores[songId].score = best;
        scores[songId].offset = best_t;
      }
    }
  }
  int best = 0, which = 0;
  for (int i = 0; i < nsongs; i++) {
    if (scores[i].score > best) {
      best = scores[i].score;
      which = i;
    }
  }
  return which;
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
  LOG_DEBUG("landmark_to_hash %.3fms", tm.getRunTime());
  const int T1_BITS = this->T1_BITS;

  size_t nmatches = 0;
  for (int i = 0; i < lms.size(); i++) {
    uint32_t key = hash[i*2];
    nmatches += db_key[key+1] - db_key[key];
  }
  // matched landmark timepoints, multithread initialize
  // cannot use std::vector as it is slow to initialize array twice
  uint32_t *matches = new uint32_t[nmatches];
  size_t pos = 0;
  #pragma omp parallel
  {
    size_t my_pos = 0;
    #pragma omp for schedule(static)
    for (int i = 0; i < lms.size(); i++) {
      uint32_t key = hash[i*2];
      my_pos += db_key[key+1] - db_key[key];
    }
    #pragma omp critical
    {
      size_t tmp = pos;
      pos += my_pos;
      my_pos = tmp;
    }
    #pragma omp for schedule(static)
    for (int i = 0; i < lms.size(); i++) {
      uint32_t key = hash[i*2];
      uint32_t t = hash[i*2+1];
      for (uint64_t it = db_key[key]; it < db_key[key+1]; it++) {
        uint32_t val = db_val[it];
        uint32_t songId = val>>T1_BITS;
        uint32_t songT = (val - t) & ((1<<T1_BITS)-1);
        matches[my_pos++] = songId<<T1_BITS | songT;
      }
    }
  }
  LOG_DEBUG("collecting matches %.3fms items %zd", tm.getRunTime(), nmatches);
  
  radix_sort_uint_omp(matches, nmatches);
  LOG_DEBUG("radix sort %.3fms", tm.getRunTime(), hist[nSongs]);
  
  int which = score_song_sorted(matches, nmatches, nSongs, out_scores);
  int all_song_score = 0;
  if (which >= 0 && which < nSongs) all_song_score = out_scores[which].score;
  LOG_DEBUG("score matches %.3fms", tm.getRunTime());
  delete[] matches;
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
