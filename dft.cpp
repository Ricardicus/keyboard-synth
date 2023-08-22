#include <complex>
#include <fftw3.h>
#include <iostream>
#include <vector>
#include <algorithm>

#include "dft.hpp"

using Complex = std::complex<double>;

std::vector<Complex> FourierTransform::DFT(const std::vector<short> &data) {
  int N = data.size();

  // Allocate memory for input and output
  fftw_complex *in =
      reinterpret_cast<fftw_complex *>(fftw_malloc(sizeof(fftw_complex) * N));
  fftw_complex *out =
      reinterpret_cast<fftw_complex *>(fftw_malloc(sizeof(fftw_complex) * N));

  // Copy input data to fftw input
  for (int i = 0; i < N; ++i) {
    in[i][0] = data[i];
    in[i][1] = 0.0;
  }

  // Plan and execute the FFT
  fftw_plan p = fftw_plan_dft_1d(N, in, out, FFTW_FORWARD, FFTW_ESTIMATE);
  fftw_execute(p);

  // Copy fftw output to std::vector<Complex>
  std::vector<Complex> result(N);
  for (int i = 0; i < N; ++i) {
    result[i] = Complex(out[i][0], out[i][1]);
  }

  // Cleanup
  fftw_destroy_plan(p);
  fftw_free(in);
  fftw_free(out);

  return result;
}

std::vector<short> FourierTransform::IDFT(const std::vector<Complex> &X) {
  int N = X.size();

  fftw_complex *in =
      reinterpret_cast<fftw_complex *>(fftw_malloc(sizeof(fftw_complex) * N));
  fftw_complex *out =
      reinterpret_cast<fftw_complex *>(fftw_malloc(sizeof(fftw_complex) * N));

  for (int i = 0; i < N; ++i) {
    in[i][0] = X[i].real();
    in[i][1] = X[i].imag();
  }

  fftw_plan p = fftw_plan_dft_1d(N, in, out, FFTW_BACKWARD, FFTW_ESTIMATE);
  fftw_execute(p);

  std::vector<short> result(N);
  for (int i = 0; i < N; ++i) {
    double val = out[i][0] / N; // Normalize by dividing by N
    result[i] = std::clamp(
        std::round(val), static_cast<double>(std::numeric_limits<short>::min()),
        static_cast<double>(std::numeric_limits<short>::max()));
  }

  fftw_destroy_plan(p);
  fftw_free(in);
  fftw_free(out);

  return result;
}
