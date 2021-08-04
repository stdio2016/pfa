#include <vector>
#include <algorithm>
#include "lib/Timing.hpp"
#include "lib/utils.hpp"
#include "Landmark.hpp"

std::vector<Landmark> LandmarkBuilder::peaks_to_landmarks(const std::vector<Peak> &peaks) {
  std::vector<Landmark> lms;
  for (int i = 0; i < peaks.size(); i++) {
    std::vector<Landmark> lm_to_sort;
    for (int j = i+1; j < peaks.size(); j++) {
      //if (j-i > FAN_COUNT) break;
      Landmark lm;
      lm.time1 = peaks[i].time;
      lm.freq1 = peaks[i].freq;
      lm.energy1 = peaks[i].energy;
      lm.time2 = peaks[j].time;
      lm.freq2 = peaks[j].freq;
      lm.energy2 = peaks[j].energy;
      int dt = lm.time2 - lm.time1;
      int df = lm.freq2 - lm.freq1;
      if (lm.time2 - lm.time1 <= LM_DT_MAX) {
        if (lm.time2 - lm.time1 >= LM_DT_MIN && abs(df) < LM_DF_MAX) {
          lm_to_sort.push_back(lm);
        }
      }
      else break;
    }
    std::sort(lm_to_sort.begin(), lm_to_sort.end(), [this](Landmark a, Landmark b) {
      // sort by energy
      return a.energy2 > b.energy2;
    });
    int cnt = 0;
    for (Landmark lm : lm_to_sort) {
      lms.push_back(lm);
      cnt += 1;
      if (cnt >= FAN_COUNT) break;
    }
  }
  return lms;
}
