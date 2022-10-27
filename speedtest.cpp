#include <chrono>
#include <experimental/random>

#include "cache_mgr.h"
#include "sort_cache_mgr.h"

#define CacheRowNum 163840
#define RandRange 1638400

using namespace std;
using namespace chrono;

template <typename T>
void print_vector_f_e(std::vector<T> v) {
  int count = 0;
  if (v.begin() != v.end()) {
    std::cout << v.front() << " ... " << v.back();
  }
  // for (auto i : v) {
  //   std::cout << i << " ";
  // }
  std::cout << std::endl;
}

void print_cache_instruction(CacheInstruction inst) {
  std::cout << "gpu_idx_vector            ";
  print_vector_f_e<long>(std::get<0>(inst));
  std::cout << "admit_cpu_idx_vector      ";
  print_vector_f_e<long>(std::get<1>(inst));
  std::cout << "admit_to_cache_idx_vector ";
  print_vector_f_e<long>(std::get<2>(inst));
  std::cout << "evict_cache_idx_vector    ";
  print_vector_f_e<long>(std::get<3>(inst));
  std::cout << "evict_to_cpu_idx_vector   ";
  print_vector_f_e<long>(std::get<4>(inst));
  std::cout << std::endl;
}

int main() {
  CacheIndicesManager mgr(CacheRowNum);
  long request[CacheRowNum];

  double cache_op_time = 0.0;
  int repeat = 100;
  for (int epoch = 0; epoch < repeat; epoch++) {
    for (int i = 0; i < CacheRowNum; i++) {
      request[i] = experimental::randint(0, RandRange - 1);
    }

    std::vector<long> request_vector(request, request + CacheRowNum);
    auto start = system_clock::now();
    auto ret = mgr.prepare_ids(request_vector);
    auto end = system_clock::now();
    auto duration = duration_cast<microseconds>(end - start);
    cache_op_time += double(duration.count()) * microseconds::period::num;
    // make sure prepare_ids is fully called, not optimized out
    print_cache_instruction(ret);
  }
  cout << "total time: " << cache_op_time / 1000 << " ms" << endl;
  cout << "average time: " << cache_op_time / 1000 / repeat << " ms" << endl;
  return 0;
}