#pragma once
#include <vector>
#include <string>
#include <omp.h>
#include "Landmark.hpp"

struct match_t {
  float score;
  float offset;
};

class Database {
public:
  // parameters
  int F1_BITS = 9;
  int DF_BITS = 8;
  int DT_BITS = 7;
  int T1_BITS = 14;
  
  std::vector<std::string> songList;
  
  std::vector<uint64_t> db_key;
  
  // use raw pointer to support memory map and pinned memory
  uint32_t *db_val;
  
  int load(std::string dir);
  
  int query_landmarks(
    const std::vector<Landmark> &lms,
    match_t *out_scores
  ) const;
  
  void landmark_to_hash(const Landmark *lms, size_t len, uint32_t song_id, uint32_t *hash_out) const;
};

template <class T>
void radix_sort_uint_omp(T *data, size_t n) {
  if (n <= 1) return ;
  //Timing tm;
  T *out = new T[n];
  const int nthreads = omp_get_max_threads();
  // 280 to prevent false sharing
  size_t *cntall = new size_t[nthreads * 280];
  for (int bt = 0; bt < sizeof(T); bt++) {
    // zero out counters
    for (int i = 0; i < nthreads * 280; i++) cntall[i] = 0;
    #pragma omp parallel
    {
      const int tid = omp_get_thread_num();
      size_t *cnt = &cntall[tid * 280];
      #pragma omp for schedule(static)
      for (long long i = 0; i < n; i++) {
        int key = data[i]>>(bt*8) & 0xff;
        cnt[key] += 1;
      }
    }
    //printf("%.1f\n", tm.getRunTime());
    size_t cum = 0;
    for (int i = 0; i < 256; i++) {
      for (int j = 0; j < nthreads; j++) {
        size_t t = cntall[i + j * 280];
        cntall[i + j * 280] = cum;
        cum += t;
      }
    }
    //printf("%.1f\n", tm.getRunTime());
    #pragma omp parallel
    {
      const int tid = omp_get_thread_num();
      size_t *cnt = &cntall[tid * 280];
      #pragma omp for schedule(static)
      for (long long i = 0; i < n; i++) {
        T dat = data[i];
        int key = dat>>(bt*8) & 0xff;
        size_t cntkey = cnt[key];
        out[cntkey] = dat;
        cnt[key] = cntkey+1;
      }
    }
    //printf("%.1f\n", tm.getRunTime());
    std::swap(out, data);
  }
  if (sizeof(T) % 2 == 1) {
    for (size_t i = 0; i < n; i++) {
      out[i] = data[i];
    }
    delete[] data;
  }
  else {
    delete[] out;
  }
  delete[] cntall;
}
