#include <algorithm>
#include <complex>
#include <fftw3.h>
#include <iostream>
#include <math.h>
#include <mutex>
#include <vector>

#include "dft.hpp"

using Complex = std::complex<double>;

// Add a static mutex for thread safety
static std::mutex fftw_mutex;

std::vector<Complex> FourierTransform::DFT(const std::vector<float> &data,
                                           bool normalize) {
  int N = data.size();

  // Scope the lock to release it as soon as possible
  fftw_complex *in, *out;
  {
    std::lock_guard<std::mutex> lock(fftw_mutex);
    in =
        reinterpret_cast<fftw_complex *>(fftw_malloc(sizeof(fftw_complex) * N));
    out =
        reinterpret_cast<fftw_complex *>(fftw_malloc(sizeof(fftw_complex) * N));
  }

  // Copy input data to fftw input (this part is thread-safe as it uses local
  // memory)
  for (int i = 0; i < N; ++i) {
    in[i][0] = data[i];
    in[i][1] = 0.0;
  }

  // Plan and execute FFT with mutex protection
  std::vector<Complex> result(N);
  {
    std::lock_guard<std::mutex> lock(fftw_mutex);
    fftw_plan p = fftw_plan_dft_1d(N, in, out, FFTW_FORWARD, FFTW_ESTIMATE);
    fftw_execute(p);

    // Copy results while still under lock
    double scale = normalize ? N : 1.0;
    for (int i = 0; i < N; ++i) {
      result[i] = Complex(out[i][0] / scale, out[i][1] / scale);
    }

    // Cleanup FFTW resources
    fftw_destroy_plan(p);
    fftw_free(in);
    fftw_free(out);
  }

  return result;
}

std::vector<Complex> FourierTransform::DFT(const std::vector<short> &data,
                                           bool normalize) {
  int N = data.size();

  fftw_complex *in, *out;
  {
    std::lock_guard<std::mutex> lock(fftw_mutex);
    in =
        reinterpret_cast<fftw_complex *>(fftw_malloc(sizeof(fftw_complex) * N));
    out =
        reinterpret_cast<fftw_complex *>(fftw_malloc(sizeof(fftw_complex) * N));
  }

  // Copy input data
  for (int i = 0; i < N; ++i) {
    in[i][0] = data[i];
    in[i][1] = 0.0;
  }

  std::vector<Complex> result(N);
  {
    std::lock_guard<std::mutex> lock(fftw_mutex);
    fftw_plan p = fftw_plan_dft_1d(N, in, out, FFTW_FORWARD, FFTW_ESTIMATE);
    fftw_execute(p);

    double scale = normalize ? N : 1.0;
    for (int i = 0; i < N; ++i) {
      result[i] = Complex(out[i][0] / scale, out[i][1] / scale);
    }

    fftw_destroy_plan(p);
    fftw_free(in);
    fftw_free(out);
  }

  return result;
}

std::vector<short> FourierTransform::IDFT(const std::vector<Complex> &X) {
  int N = X.size();

  fftw_complex *in, *out;
  {
    std::lock_guard<std::mutex> lock(fftw_mutex);
    in =
        reinterpret_cast<fftw_complex *>(fftw_malloc(sizeof(fftw_complex) * N));
    out =
        reinterpret_cast<fftw_complex *>(fftw_malloc(sizeof(fftw_complex) * N));
  }

  // Copy input data
  for (int i = 0; i < N; ++i) {
    in[i][0] = X[i].real();
    in[i][1] = X[i].imag();
  }

  std::vector<short> result(N);
  {
    std::lock_guard<std::mutex> lock(fftw_mutex);
    fftw_plan p = fftw_plan_dft_1d(N, in, out, FFTW_BACKWARD, FFTW_ESTIMATE);
    fftw_execute(p);

    for (int i = 0; i < N; ++i) {
      double val = out[i][0] / N;
      result[i] =
          std::clamp(std::round(val),
                     static_cast<double>(std::numeric_limits<short>::min()),
                     static_cast<double>(std::numeric_limits<short>::max()));
    }

    fftw_destroy_plan(p);
    fftw_free(in);
    fftw_free(out);
  }

  return result;
}
