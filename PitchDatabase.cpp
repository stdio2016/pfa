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
    std::string src;
    std::getline(fin, src);
    std::string line;
    std::getline(fin, line);
    int ptc;
    std::stringstream ss; ss << line;
    int i = 0;
    std::vector<int> pitch;
    while (ss >> ptc) {
      pitch.push_back(ptc);
      i++;
    }
    if (i == 0) continue;
    std::vector<int> tosort = pitch;
    // auto transpose!
    std::sort(tosort.begin(), tosort.end());
    int mid = tosort[tosort.size()/2];
    std::cout << "db " << name << " mid=" << mid << '\n';
    srcList.push_back(src);
    songList.push_back(name);
    pitches.push_back(pitch);
    midPitches.push_back(mid);
  }
  fin.close();
  return 0;
}

double computeDtw(const std::vector<int> &song, const std::vector<int> &seq, int shift) {
  if (song.size() == 0 || seq.size() == 0) return 99999;
  int n = song.size();
  std::vector<int> go(n+1,99999), go2(n+1,99999);
  go[0] = 0;
  for (int i = 0; i < seq.size(); i++) {
    go2[0] = 99999;
    for (int j = 0; j < n; j++) {
      int cho1 = go2[j];
      int cho2 = go[j+1];
      int cho3 = go[j];
      int diff = song[j] - seq[i] + shift;
      if (diff < 0) diff = -diff;
      go2[j+1] = diff + std::min(cho1, std::min(cho2, cho3));
      //go2[j] = diff + std::min(cho1, cho2);
    }
    std::swap(go, go2);
  }
  double best = 99999;
  for (int i = 90; i < n; i++) {
    if ((double)go[i]/(i+seq.size()) < best) best = (double)go[i]/(i+seq.size());
  }
  return best;
}

int PitchDatabase::query_pitch(
  const std::vector<int> &pitch,
  double *out_scores,
  FILE *log_file
) const {
  std::vector<int> tosort = pitch;
  // auto transpose!
  std::sort(tosort.begin(), tosort.end());
  int mid = tosort[tosort.size()/2];
  printf("query mid=%d\n", mid);
  for (int i = 0; i < songList.size(); i++) {
    int which = 0;
    double best = 99999;
    double cost = computeDtw(pitches[i], pitch, mid - midPitches[i]);
    double cost2 = computeDtw(pitches[i], pitch, mid - midPitches[i] + 1);
    if (cost < cost2) {
      while (cost < cost2) {
        which -= 1;
        cost2 = cost;
        cost = computeDtw(pitches[i], pitch, mid - midPitches[i] + which);
      }
      which += 1;
      best = cost2;
    }
    else {
      which = 1;
      while (cost2 < cost) {
        which += 1;
        cost = cost2;
        cost2 = computeDtw(pitches[i], pitch, mid - midPitches[i] + which);
      }
      which -= 1;
      best = cost;
    }
    out_scores[i] = -best;
    printf("  %s %f shift=%d\n", srcList[i].c_str(), best, which);
    //std::cout << "  " << srcList[i] <<' ' << best << " shift=" << which << '\n';
  }
  return 0;
}
