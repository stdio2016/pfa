#pragma once

struct PitchTracking {
  int FFT_SIZE = 2048;
  double StepTime = 0.01;
  double MinimumPitch;
  double MaximumPitch;
  // parameter from Praat source code
  // https://github.com/praat/praat/blob/master/fon/Sound_to_Pitch.cpp#L566
  double OctaveCost = 0.01;
  double VoicingThreshold = 0.45;
  double SilenceThreshold = 0.03;
  double VoicedUnvoicedCost = 0.14;
  double OctaveJumpCost = 0.35;
  int MaxCandidates = 15;
  std::vector<double> hannWindow;
  std::vector<double> hannAuto;
  PitchTracking() {
    MinimumPitch = 55 * pow(2, 2.5/12);
    MaximumPitch = 880 * pow(2, 3.5/12);
  }
};

std::vector<double> getPitch(std::vector<float> buf, int smpRate, PitchTracking param);
