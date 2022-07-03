#pragma once
#include <vector>
#include <cmath>
#include <stdexcept>

template<typename F=double>
class FFT {
private:
  unsigned N;
  std::vector<F> trig;
  std::vector<unsigned> perm;
  void permute(const F *din, F *dout) const;
  void permuteInPlace(F *d) const;
public:
  // FFT of N complex numbers or 2N real numbers
  FFT(unsigned N);

  // FFT/IFFT of N complex numbers
  void transform(const F *din, F *dout, bool inverse) const;

  /** 
  * FFT of 2N real numbers
  * output format: 
  *     0: F[0] re
  *     1: F[N/2] re
  *    2k: F[k] re, for k>=1
  *  2k+1: F[k] im, for k>=1
  */
  void realFFT(const F *din, F *dout) const;
  
  // IFFT of 2N real numbers
  void realIFFT(const F *din, F* dout) const;
};

template<typename F>
FFT<F>::FFT(unsigned N): N(N) {
  if (N == 0 || (N&N-1) != 0) {
    throw std::runtime_error("N must be power of 2");
  }
  if (N >= (1<<30)) {
    throw std::runtime_error("N is too big!");
  }
  const F PI = 3.14159265358979323846;
  trig.resize(N*3);
  for (unsigned i = 2; i < N; i *= 2) {
    for (unsigned j = 0; j < i/2; j++) {
      trig[(i+j)*2] = cos(PI / i * j);
      trig[(i+j)*2+1] = sin(PI / i * j);
    }
    for (unsigned j = i/2; j < i; j++) {
      trig[(i+j)*2] = -trig[i+j*2+1];
      trig[(i+j)*2+1] = trig[i+j*2];
    }
  }

  unsigned i = N;
  for (unsigned j = 0; j < N/2; j++) {
    trig[(i+j)*2] = cos(PI / i * j);
    trig[(i+j)*2+1] = sin(PI / i * j);
  }
  
  perm.resize(N);
  perm[0] = 0;
  for (unsigned i = 1, B = N/2; i < N; i *= 2, B >>= 1) {
    for (unsigned j = 0; j < i; j++) {
      perm[i+j] = perm[j] | B;
    }
  }
}

template<typename F>
void FFT<F>::permute(const F *din, F *dout) const {
  for (unsigned i = 0; i < N; i++) {
    unsigned p = perm[i];
    dout[p*2] = din[i*2];
    dout[p*2+1] = din[i*2+1];
  }
}

template<typename F>
void FFT<F>::permuteInPlace(F *d) const {
  for (unsigned i = 0; i < N; i++) {
    unsigned p = perm[i];
    if (p < i) {
      F t1 = d[i*2];
      F t2 = d[i*2+1];
      d[i*2] = d[p*2];
      d[i*2+1] = d[p*2+1];
      d[p*2] = t1;
      d[p*2+1] = t2;
    }
  }
}

template<typename F>
void FFT<F>::transform(const F* din, F* dout, bool inverse) const {
  unsigned n = N * 2;
  if (din == dout) {
    permuteInPlace(dout);
  }
  else {
    permute(din, dout);
  }
  
  // first round
  for (unsigned i = 0; i < n; i += 4) {
    F a = dout[i];
    F b = dout[i+1];
    F c = dout[i+2];
    F d = dout[i+3];
    dout[i] = a + c;
    dout[i+1] = b + d;
    dout[i+2] = a - c;
    dout[i+3] = b - d;
  }
  
  // more round
  for (unsigned step = 4; step < n; step *= 2) {
    for (unsigned j = step; j < n; j += step) {
      for (unsigned i = 0; i < step; i+=2) {
        F a = dout[j];
        F b = dout[j+1];
        j -= step;
        F sin = trig[step+i+1];
        sin = inverse ? sin : -sin;
        F c = a * trig[step+i] - b * sin;
        F d = a * sin + b * trig[step+i];
        a = dout[j];
        b = dout[j+1];
        dout[j] = a + c;
        dout[j+1] = b + d;
        j += step;
        dout[j] = a - c;
        dout[j+1] = b - d;
        j += 2;
      }
    }
  }
}

template<typename F>
void FFT<F>::realFFT(const F* din, F* dout) const {
  unsigned n = N;
  transform(din, dout, false);
  F a = dout[0];
  F b = dout[1];
  dout[0] = a + b; // real part of F[0]
  dout[1] = a - b; // real part of F[N/2]
  for (unsigned k = 1; k < N/2; k++) {
    F fk = dout[k*2];
    F fki = dout[k*2+1];
    F fnk = dout[(n-k)*2];
    F fnki = dout[(n-k)*2+1];
    
    F cos = trig[(n+k)*2];
    F sin = trig[(n+k)*2+1];
    a = fki + fnki;
    b = fnk - fk;
    F c = a * cos + b * sin;
    F d = b * cos - a * sin;
    a = fk + fnk;
    b = fki - fnki;
    F half = 0.5;
    dout[k*2] = (a + c) * half;
    dout[k*2+1] = (d + b) * half;
    dout[(n-k)*2] = (a - c) * half;
    dout[(n-k)*2+1] = (d - b) * half;
  }
  dout[n+1] = -dout[n+1];
}

template<typename F>
void FFT<F>::realIFFT(const F* din, F* dout) const {
  F a, b, c, d;
  unsigned n = N;
  a = din[0];
  b = din[1];
  dout[0] = (a + b);
  dout[1] = (a - b);
  for (unsigned k = 1; k < n/2; k++) {
    F kr = din[k*2];
    F ki = din[k*2+1];
    F nr = din[(n-k)*2];
    F ni = din[(n-k)*2+1];
    
    F cos = trig[(n+k)*2];
    F sin = trig[(n+k)*2+1];
    
    a = (kr - nr);
    b = (ki + ni);
    c = a * cos - b * sin;
    d = a * sin + b * cos;
    a = (kr + nr);
    b = (ki - ni);
    
    dout[k*2] = a - d;
    dout[k*2+1] = b + c;
    dout[(n-k)*2] = a + d;
    dout[(n-k)*2+1] = c - b;
  }
  dout[n] = din[n] * 2;
  dout[n+1] = -din[n+1] * 2;
  transform(dout, dout, true);
}
