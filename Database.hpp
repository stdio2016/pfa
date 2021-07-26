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
  std::vector<std::string> songList;
  std::vector<std::string> songNameList;
  std::vector<std::string> songSrcList;
  
  std::vector<long long> db_key;
  
  std::vector<uint32_t> db_val;
  
  int load(std::string dir);
  
  int query_landmarks(
    const std::vector<Landmark> &lms,
    match_t *out_scores
  ) const;
};
