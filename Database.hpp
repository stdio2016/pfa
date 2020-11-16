#pragma once
#include <vector>
#include <string>
#include "Landmark.hpp"

struct match_t {
  int offset;
  int score;
};

class Database {
public:
  std::vector<std::string> songList;
  
  std::vector<long long> db_key;
  
  std::vector<uint32_t> db_val;
  
  int load(std::string dir);
  
  int query_landmarks(
    const std::vector<Landmark> &lms,
    match_t *out_scores,
    FILE *log_file
  ) const;
};
