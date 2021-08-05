#pragma once
#include <vector>
#include <string>
#include "Landmark.hpp"

struct match_t {
  int32_t offset;
  int32_t score;
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
  
  std::vector<uint32_t> db_val;
  
  int load(std::string dir);
  
  int query_landmarks(
    const std::vector<Landmark> &lms,
    match_t *out_scores
  ) const;
  
  void landmark_to_hash(const Landmark *lms, size_t len, uint32_t song_id, uint32_t *hash_out) const;
};
