#ifndef FOURIER_TRANSFORM_HPP
#define FOURIER_TRANSFORM_HPP

#include <complex>
#include <vector>

class FourierTransform {
private:
  using Complex = std::complex<double>;

public:
  // Computes the Discrete Fourier Transform of the input data
  static std::vector<Complex> DFT(const std::vector<short> &data,
                                  bool normalize);
  static std::vector<Complex> DFT(const std::vector<float> &data,
                                  bool normalize);
  // Computes the Inverse Discrete Fourier Transform of the input data
  static std::vector<short> IDFT(const std::vector<Complex> &X);
};

#endif // FOURIER_TRANSFORM_HPP
