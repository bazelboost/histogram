// Copyright 2015-2018 Hans Dembinski
//
// Distributed under the Boost Software License, version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HISTOGRAM_ACCUMULATORS_MEAN_HPP
#define BOOST_HISTOGRAM_ACCUMULATORS_MEAN_HPP

#include <boost/core/nvp.hpp>
#include <boost/histogram/detail/square.hpp>
#include <boost/histogram/fwd.hpp> // for mean<>
#include <boost/throw_exception.hpp>
#include <cassert>
#include <stdexcept>
#include <type_traits>

namespace boost {
namespace histogram {
namespace accumulators {

/** Calculates mean and variance of sample.

  Uses Welfords's incremental algorithm to improve the numerical
  stability of mean and variance computation.
*/
template <class ValueType>
class mean {
public:
  using value_type = ValueType;
  using const_reference = const value_type&;

  struct impl_type {
    value_type sum_;
    value_type mean_;
    value_type sum_of_deltas_squared_;
  };

  mean() = default;

  /// Allow implicit conversion from mean<T>.
  template <class T>
  mean(const mean<T>& o) noexcept : data_{o.data_} {}

  /// Initialize to external count, mean, and variance.
  mean(const_reference n, const_reference mean, const_reference variance) noexcept
      : data_{n, mean, variance * (n - 1)} {}

  /// Insert sample x.
  void operator()(const_reference x) noexcept {
    data_.sum_ += static_cast<value_type>(1);
    const auto delta = x - data_.mean_;
    data_.mean_ += delta / data_.sum_;
    data_.sum_of_deltas_squared_ += delta * (x - data_.mean_);
  }

  /// Insert sample x with weight w.
  void operator()(const weight_type<value_type>& w, const_reference x) noexcept {
    data_.sum_ += w.value;
    const auto delta = x - data_.mean_;
    data_.mean_ += w.value * delta / data_.sum_;
    data_.sum_of_deltas_squared_ += w.value * delta * (x - data_.mean_);
  }

  /// Add another mean accumulator.
  mean& operator+=(const mean& rhs) noexcept {
    if (rhs.data_.sum_ == 0) return *this;

    /*
      sum_of_deltas_squared
        = sum_i (x_i - mu)^2
        = sum_i (x_i - mu)^2 + sum_k (x_k - mu)^2
        = sum_i (x_i - mu1 + (mu1 - mu))^2 + sum_k (x_k - mu2 + (mu2 - mu))^2

      first part:
      sum_i (x_i - mu1 + (mu1 - mu))^2
        = sum_i (x_i - mu1)^2 + n1 (mu1 - mu))^2 + 2 (mu1 - mu) sum_i (x_i - mu1)
        = sum_i (x_i - mu1)^2 + n1 (mu1 - mu))^2
      since sum_i (x_i - mu1) = n1 mu1 - n1 mu1 = 0

      Putting it together:
      sum_of_deltas_squared
        = sum_of_deltas_squared_1 + n1 (mu - mu1))^2
        + sum_of_deltas_squared_2 + n2 (mu - mu2))^2
    */

    const auto n1 = data_.sum_;
    const auto mu1 = data_.mean_;
    const auto n2 = rhs.data_.sum_;
    const auto mu2 = rhs.data_.mean_;

    data_.sum_ += rhs.data_.sum_;
    data_.mean_ = (n1 * mu1 + n2 * mu2) / data_.sum_;
    data_.sum_of_deltas_squared_ += rhs.data_.sum_of_deltas_squared_;
    data_.sum_of_deltas_squared_ += n1 * detail::square(data_.mean_ - mu1);
    data_.sum_of_deltas_squared_ += n2 * detail::square(data_.mean_ - mu2);

    return *this;
  }

  /** Scale by value.

   This acts as if all samples were scaled by the value.
  */
  mean& operator*=(const_reference s) noexcept {
    data_.mean_ *= s;
    data_.sum_of_deltas_squared_ *= s * s;
    return *this;
  }

  bool operator==(const mean& rhs) const noexcept {
    return data_.sum_ == rhs.data_.sum_ && data_.mean_ == rhs.data_.mean_ &&
           data_.sum_of_deltas_squared_ == rhs.data_.sum_of_deltas_squared_;
  }

  bool operator!=(const mean& rhs) const noexcept { return !operator==(rhs); }

  /** Return how many samples were accumulated.

    count() should be used to check whether value() and variance() are defined,
    see documentation of value() and variance(). count() can be used to compute
    the variance of the mean by dividing variance() by count().
  */
  const_reference count() const noexcept { return data_.sum_; }

  /** Return mean value of accumulated samples.

    The result is undefined, if `count() < 1`.
  */
  const_reference value() const noexcept { return data_.mean_; }

  /** Return variance of accumulated samples.

    The result is undefined, if `count() < 2`.
  */
  value_type variance() const noexcept {
    return data_.sum_of_deltas_squared_ / (data_.sum_ - 1);
  }

  template <class Archive>
  void serialize(Archive& ar, unsigned version) {
    if (version == 0) {
      // read only
      std::size_t sum;
      ar& make_nvp("sum", sum);
      data_.sum_ = static_cast<value_type>(sum);
    } else {
      ar& make_nvp("sum", data_.sum_);
    }
    ar& make_nvp("mean", data_.mean_);
    ar& make_nvp("sum_of_deltas_squared", data_.sum_of_deltas_squared_);
  }

private:
  impl_type data_{0, 0, 0};

  friend struct ::boost::histogram::unsafe_access;
};

} // namespace accumulators
} // namespace histogram
} // namespace boost

#ifndef BOOST_HISTOGRAM_DOXYGEN_INVOKED

namespace boost {
namespace serialization {

template <class T>
struct version;

// version 1 for boost::histogram::accumulators::mean<T>
template <class T>
struct version<boost::histogram::accumulators::mean<T>> : std::integral_constant<int, 1> {
};

} // namespace serialization
} // namespace boost

namespace std {
template <class T, class U>
/// Specialization for boost::histogram::accumulators::mean.
struct common_type<boost::histogram::accumulators::mean<T>,
                   boost::histogram::accumulators::mean<U>> {
  using type = boost::histogram::accumulators::mean<common_type_t<T, U>>;
};
} // namespace std

#endif

#endif
