#pragma once
#include <vector>
#include <string>
#include "Landmark.hpp"

struct match_t {
  int offset;
  int score;
};

class PitchDatabase {
public:
  std::vector<std::string> songList;
  std::vector<std::string> srcList;
  
  std::vector<std::vector<int> > pitches;
  
  std::vector<int> midPitches;
  
  int load(std::string dir);
  
  int query_pitch(
    const std::vector<int> &pitch,
    double *out_scores,
    FILE *log_file
  ) const;
};
