#include "fft.hpp"
#include <algorithm>
#include <cmath>
#include "Pitch.hpp"

struct PitchCandidate {
  double frequency, strength;
};

std::vector<PitchCandidate> getSegmentCandidates(
  float *buf,
  int pos,
  FFT<double> fft,
  double smpRate,
  double globalVol,
  const PitchTracking &param
) {
  double vol = 0;
  int size = param.FFT_SIZE;
  // multiply with Hann window
  std::vector<double> smp(size * 2);
  double sum = 0;
  for (int i = 0; i < size; i++) {
    sum += buf[pos+i];
  }
  sum /= size;
  for (int i = 0; i < size; i++) {
    smp[i] = buf[pos+i] - sum;
  }
  for (int i = 0; i < size; i++) {
    double b = smp[i];
    vol = std::max(vol, fabs(b));
    smp[i] = b * param.hannWindow[i];
  }
  fft.realFFT(smp.data(), smp.data());
  for (int i = 1; i < size; i++) {
    double re = smp[i*2];
    double im = smp[i*2+1];
    smp[i*2] = re*re + im*im;
    smp[i*2+1] = 0;
  }
  smp[1] = smp[1] * smp[1];
  smp[0] = smp[0] * smp[0];
  // corr[i*2] is autocorrelation
  std::vector<double> corr(size * 2);
  fft.realIFFT(smp.data(), corr.data());
  double normalize = 1.0/corr[0];
  for (int i = 0; i < size; i++) {
    smp[i+size] = corr[i] * normalize / param.hannAuto[i];
  }
  for (int i = 1; i < size; i++) {
    smp[size-i] = corr[i] * normalize / param.hannAuto[i];
  }
  int lim = smpRate/param.MinimumPitch;
  double silenceR = param.VoicingThreshold + std::max(0.0,
    2.0 - (vol/globalVol) / (param.SilenceThreshold/(1.0+param.VoicingThreshold)));
  std::vector<PitchCandidate> candidates(param.MaxCandidates+1);
  candidates[0] = PitchCandidate{0, silenceR};
  for (int i = 0; i < param.MaxCandidates; i++) {
    candidates[i+1] = PitchCandidate{0, 0};
  }
  for (int i = smpRate/param.MaximumPitch; i < lim; i++) {
    double r0 = smp[size-1+i];
    double r = smp[size+i];
    double r1 = smp[size+1+i];
    if (r0 > r || r < r1) continue;
    double peak = r + (r0-r1) * (r0-r1) * 0.125 / (2*r - r0 - r1); 
    double delta =  i + (r0 - r1) * 0.5 / (r0 + r1 - 2 * r);
    double R = peak - param.OctaveCost * log2(param.MinimumPitch/smpRate * delta);
    int nn = param.MaxCandidates;
    while (nn >= 0 && R > candidates[nn].strength) {
      if (nn < param.MaxCandidates) candidates[nn+1] = candidates[nn];
      nn--;
    }
    if (nn < param.MaxCandidates) {
      candidates[nn+1] = PitchCandidate{smpRate/delta, R};
    }
  }
  return candidates;
}

std::vector<double> getPitch(
  std::vector<float> buf,
  int smpRate,
  PitchTracking param
) {
  std::vector<std::vector<PitchCandidate>> candidates;
  int stepTime = smpRate * param.StepTime;
  int MaxCandidates = param.MaxCandidates;
  int fftSize = param.FFT_SIZE;
  FFT<double> fft(fftSize);
  int i = 0;
  // compute global maximum
  double globalVol = 0;
  for (int i = 0; i < buf.size(); i++) {
    globalVol = std::max(globalVol, (double)fabs(buf[i]));
  }
  
  // hann window
  param.hannWindow.resize(fftSize);
  param.hannAuto.resize(fftSize);
  double PI = 3.14159;
  double k = 2.0 * PI / fftSize;
  for (int i = 0; i < fftSize; i++) {
    double t = (double)i / fftSize;
    param.hannWindow[i] = 0.5 - 0.5 * cos(k * i);
    param.hannAuto[i] = (1.0 - t) * (2.0/3.0 + 1.0/3.0 * cos(k*i)) + 1.0/(2.0*PI) * sin(k*i);
  }
  
  // compute pitch candidates
  for (int pos = 0; pos + fftSize < buf.size(); pos += stepTime) {
    candidates.push_back(getSegmentCandidates(
      buf.data(),
      pos,
      fft,
      smpRate,
      globalVol,
      param
    ));
  }
  
  int nFrames = candidates.size();
  if (nFrames == 0) {
    return std::vector<double>(0);
  }
  std::vector<double> cost(candidates.size() * (MaxCandidates + 1));
  std::vector<int> back(candidates.size() * (MaxCandidates + 1));
  for (int i = 0; i <= MaxCandidates; i++) {
    cost[i] = -candidates[0][i].strength;
  }
  for (int t = 1; t < nFrames; t++) {
    for (int i = 0; i <= MaxCandidates; i++) {
      PitchCandidate next = candidates[t][i];
      double best = 999;
      int which = 0;
      for (int j = 0; j <= MaxCandidates; j++) {
        PitchCandidate prev = candidates[t-1][j];
        double newCost = cost[(t-1)*(MaxCandidates+1) + j] - next.strength;
        if (prev.frequency == 0 && next.frequency == 0) {
          // unvoiced
          newCost += 0;
        }
        else if (prev.frequency != 0 && next.frequency != 0) {
          // voiced
          newCost += param.OctaveJumpCost * fabs(log2(next.frequency / prev.frequency));
        }
        else {
          // voice -> unvoice trnsition
          newCost += param.VoicedUnvoicedCost;
        }
        
        if (j == 0 || newCost < best) {
          best = newCost;
          which = j;
        }
      }
      cost[t*(MaxCandidates+1)+i] = best;
      back[t*(MaxCandidates+1)+i] = which;
    }
  }
  
  // backtrack
  std::vector<double> pitch;
  int Q = 0;
  double best = 999;
  for (int i = 0; i <= MaxCandidates; i++) {
    if (i == 0 || cost[(nFrames-1)*(MaxCandidates+1)+i] < best) {
      best = cost[(nFrames-1)*(MaxCandidates+1)+i];
      Q = i;
    }
  }
  for (int t = nFrames-1; t > 0; t--) {
    PitchCandidate choose = candidates[t][Q];
    double ans = choose.frequency;
    pitch.push_back(ans);
    Q = back[t*(MaxCandidates+1)+Q];
  }
  // last step
  {
    PitchCandidate choose = candidates[0][Q];
    double ans = choose.frequency;
    pitch.push_back(ans);
  }
  std::reverse(pitch.begin(), pitch.end());
  return pitch;
}
