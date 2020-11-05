#include <vector>

std::vector<float> lopass(const std::vector<float> &snd, double ratio, int firSize);
std::vector<float> resample(const std::vector<float> &snd, int from, int to);
