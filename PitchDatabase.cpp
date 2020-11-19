#include "PitchDatabase.hpp"
#include <fstream>
#include <algorithm>
#include <sstream>
#include <iostream>

int PitchDatabase::load(std::string dir) {
  std::ifstream fin(dir);
  if (!fin) return 1;
  std::string name;
  while (std::getline(fin, name)) {
    std::string line;
    std::getline(fin, line);
    int ptc;
    std::stringstream ss; ss << line;
    int i = 0;
    std::vector<int> pitch;
    while (ss >> ptc) {
      if (i%5 == 0)
      pitch.push_back(ptc);
      i++;
    }
    if (i == 0) continue;
    std::vector<int> tosort = pitch;
    // auto transpose!
    std::sort(tosort.begin(), tosort.end());
    int mid = tosort[tosort.size()/2];
    std::cout << "db " << name << " mid=" << mid << '\n';
    songList.push_back(name);
    pitches.push_back(pitch);
    midPitches.push_back(mid);
  }
  fin.close();
  return 0;
}

int computeDtw(const std::vector<int> &song, const std::vector<int> &seq, int shift) {
  if (song.size() == 0 || seq.size() == 0) return 99999;
  int n = seq.size();
  std::vector<int> go(n+1,99999), go2(n+1,99999);
  go[0] = 0;
  for (int i = 0; i < song.size(); i++) {
    go2[0] = 99999;
    for (int j = 0; j < n; j++) {
      int cho1 = go2[j];
      int cho2 = go[j+1];
      int cho3 = go[j];
      int diff = song[i] - seq[j] + shift;
      if (diff < 0) diff = -diff;
      go2[j+1] = diff + std::min(cho1, std::min(cho2, cho3));
      //go2[j] = diff + std::min(cho1, cho2);
    }
    std::swap(go, go2);
  }
  return go[n];
}

int PitchDatabase::query_pitch(
  const std::vector<int> &pitch,
  int *out_scores,
  FILE *log_file
) const {
  std::vector<int> tosort = pitch;
  // auto transpose!
  std::sort(tosort.begin(), tosort.end());
  int mid = tosort[tosort.size()/2];
  std::cout << "query mid=" << mid << '\n';
  for (int i = 0; i < songList.size(); i++) {
    int which = 0, best = 99999;
    for (int shift = -2; shift <= 2; shift++) {
      int cost = computeDtw(pitches[i], pitch, shift + mid - midPitches[i]);
      if (cost < best) {
        which = shift;
        best = cost;
      }
    }
    out_scores[i] = -best;
    std::cout << "  " << songList[i] <<' ' << best << " shift=" << which << '\n';
  }
  return 0;
}
