#pragma once

#include <cmath>
#include <cstdint>
#include <random>

namespace minera {

// The legacy global generator of numpy (numpy.random.seed with an integer and
// the RandomState draws): MT19937 seeded by init_genrand, 53-bit doubles from
// two 32-bit outputs, the polar normal with its cached second value, and the
// legacy exponential, Laplace and gamma samplers. These are the published
// numpy algorithms; draws match numpy bit for bit.
class NumpyLegacyRandom {
 public:
  explicit NumpyLegacyRandom(std::uint32_t seed) : engine_(seed) {}

  double random_sample() {
    const auto a = engine_() >> 5;
    const auto b = engine_() >> 6;
    return (static_cast<double>(a) * 67108864.0 + static_cast<double>(b)) / 9007199254740992.0;
  }

  double gauss() {
    if (has_gauss_) {
      has_gauss_ = false;
      return gauss_;
    }
    double x1, x2, r2;
    do {
      x1 = 2.0 * random_sample() - 1.0;
      x2 = 2.0 * random_sample() - 1.0;
      r2 = x1 * x1 + x2 * x2;
    } while (r2 >= 1.0 || r2 == 0.0);
    const double f = std::sqrt(-2.0 * std::log(r2) / r2);
    gauss_ = f * x1;
    has_gauss_ = true;
    return f * x2;
  }

  double standard_exponential() { return -std::log(1.0 - random_sample()); }

  double laplace(double loc, double scale) {
    for (;;) {
      const double u = random_sample();
      if (u >= 0.5) return loc - scale * std::log(2.0 - u - u);
      if (u > 0.0) return loc + scale * std::log(u + u);
    }
  }

  double standard_gamma(double shape) {
    if (shape == 1.0) return standard_exponential();
    if (shape == 0.0) return 0.0;
    if (shape < 1.0) {
      for (;;) {
        const double u = random_sample();
        const double v = standard_exponential();
        if (u <= 1.0 - shape) {
          const double x = std::pow(u, 1.0 / shape);
          if (x <= v) return x;
        } else {
          const double y = -std::log((1.0 - u) / shape);
          const double x = std::pow(1.0 - shape + shape * y, 1.0 / shape);
          if (x <= v + y) return x;
        }
      }
    }
    const double b = shape - 1.0 / 3.0;
    const double c = 1.0 / std::sqrt(9.0 * b);
    for (;;) {
      double x, v;
      do {
        x = gauss();
        v = 1.0 + c * x;
      } while (v <= 0.0);
      v = v * v * v;
      const double u = random_sample();
      if (u < 1.0 - 0.0331 * (x * x) * (x * x)) return b * v;
      if (std::log(u) < 0.5 * x * x + b * (1.0 - v + std::log(v))) return b * v;
    }
  }

 private:
  std::mt19937 engine_;
  bool has_gauss_ = false;
  double gauss_ = 0.0;
};

}  // namespace minera
