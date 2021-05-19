//==============================================================
// Copyright © 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

// oneDPL headers should be included before standard headers
#include <oneapi/dpl/algorithm>
#include <oneapi/dpl/execution>
#include <oneapi/dpl/numeric>

#include <CL/sycl.hpp>
#include <iostream>
#include <random>

// Dense algorithm stores all the bins, even if bin has 0 entries
// input array [4,4,1,0,1,2]
// output [(0,1) (1,2)(2,1)(3,0)(4,2)]
// On the other hand, the sparse algorithm excludes the zero-bin values
// i.e., for the sparse algorithm, the same input will give the following output
// [(0,1) (1,2)(2,1)(4,2)]

void dense_histogram(std::vector<uint64_t> &input) {
  const int N = input.size();
  sycl::buffer<uint64_t> histogram_buf{input.data(), sycl::range<1>(N)};

  // Combine the equal values together
  std::sort(oneapi::dpl::execution::dpcpp_default,
            oneapi::dpl::begin(histogram_buf), oneapi::dpl::end(histogram_buf));

  // num_bins is maximum value + 1
  int num_bins;
  {
    sycl::host_accessor histogram(histogram_buf, sycl::read_only);
    num_bins = histogram[N - 1] + 1;
  }
  sycl::buffer<uint64_t> histogram_new_buf{sycl::range<1>(num_bins)};
  auto val_begin = oneapi::dpl::counting_iterator<int>{0};

  // Determine the end of each bin of value
  oneapi::dpl::upper_bound(
      oneapi::dpl::execution::dpcpp_default, oneapi::dpl::begin(histogram_buf),
      oneapi::dpl::end(histogram_buf), val_begin, val_begin + num_bins,
      oneapi::dpl::begin(histogram_new_buf));

  // Compute histogram by calculating differences of cumulative histogram
  std::adjacent_difference(oneapi::dpl::execution::dpcpp_default,
                           oneapi::dpl::begin(histogram_new_buf),
                           oneapi::dpl::end(histogram_new_buf),
                           oneapi::dpl::begin(histogram_new_buf));

  std::cout << "success for Dense Histogram:\n";
  {
    sycl::host_accessor histogram_new(histogram_new_buf, sycl::read_only);
    std::cout << "[";
    for (int i = 0; i < num_bins; i++) {
      std::cout << "(" << i << ", " << histogram_new[i] << ") ";
    }
    std::cout << "]\n";
  }
}

void sparse_histogram(std::vector<uint64_t> &input) {
  const int N = input.size();
  sycl::buffer<uint64_t> histogram_buf{input.data(), sycl::range<1>(N)};

  // Combine the equal values together
  std::sort(oneapi::dpl::execution::dpcpp_default,
            oneapi::dpl::begin(histogram_buf), oneapi::dpl::end(histogram_buf));

  auto num_bins = std::transform_reduce(
      oneapi::dpl::execution::dpcpp_default, oneapi::dpl::begin(histogram_buf),
      oneapi::dpl::end(histogram_buf), oneapi::dpl::begin(histogram_buf) + 1, 1,
      std::plus<int>(), std::not_equal_to<int>());

  // Create new buffer to store the unique values and their count
  sycl::buffer<uint64_t> histogram_values_buf{sycl::range<1>(num_bins)};
  sycl::buffer<uint64_t> histogram_counts_buf{sycl::range<1>(num_bins)};

  sycl::buffer<uint64_t> _const_buf{sycl::range<1>(N)};
  std::fill(oneapi::dpl::execution::dpcpp_default,
            oneapi::dpl::begin(_const_buf), oneapi::dpl::end(_const_buf), 1);

  // Find the count of each value
  oneapi::dpl::reduce_by_segment(
      oneapi::dpl::execution::dpcpp_default, oneapi::dpl::begin(histogram_buf),
      oneapi::dpl::end(histogram_buf), oneapi::dpl::begin(_const_buf),
      oneapi::dpl::begin(histogram_values_buf),
      oneapi::dpl::begin(histogram_counts_buf));

  std::cout << "success for Sparse Histogram:\n";
  std::cout << "[";
  for (int i = 0; i < num_bins - 1; i++) {
    sycl::host_accessor histogram_value(histogram_values_buf, sycl::read_only);
    sycl::host_accessor histogram_count(histogram_counts_buf, sycl::read_only);
    std::cout << "(" << histogram_value[i] << ", " << histogram_count[i]
              << ") ";
  }
  std::cout << "]\n";
}

int main(void) {
  const int N = 1000;
  std::vector<uint64_t> input;
  srand((unsigned)time(0));
  // initialize the input array with randomly generated values between 0 and 9
  for (int i = 0; i < N; i++) input.push_back(rand() % 9);

  // replacing all input entries of "4" with random number between 1 and 3
  // this is to ensure that we have atleast one entry with zero-bin size,
  // which shows the difference between sparse and dense algorithm output
  for (int i = 0; i < N; i++)
    if (input[i] == 4) input[i] = rand() % 3;
  dense_histogram(input);
  sparse_histogram(input);
  return 0;
}