#include "cache_mgr.h"
#include "sort_cache_mgr.h"
template <typename T>
void print_vector(std::vector<T> v) {
  for (auto i : v) {
    std::cout << i << " ";
  }
  std::cout << std::endl;
}

void print_cache_instruction(CacheInstruction inst) {
  std::cout << "gpu_idx_vector            ";
  print_vector<long>(std::get<0>(inst));
  std::cout << "admit_cpu_idx_vector      ";
  print_vector<long>(std::get<1>(inst));
  std::cout << "admit_to_cache_idx_vector ";
  print_vector<long>(std::get<2>(inst));
  std::cout << "evict_cache_idx_vector    ";
  print_vector<long>(std::get<3>(inst));
  std::cout << "evict_to_cpu_idx_vector   ";
  print_vector<long>(std::get<4>(inst));
  std::cout << std::endl;
}

void op(SortCacheIndicesManager& mgr, long request[], long n) {
  std::cout << "incoming request: ";
  for (long i = 0; i < n; i++) {
    std::cout << request[i] << " ";
  }
  std::cout << std::endl;
  std::vector<long> request_vector(request, request + n);
  auto ret = mgr.prepare_ids(request_vector);
  print_cache_instruction(ret);
}

int main() {
  SortCacheIndicesManager mgr(4);
  {
    long request[] = {0, 1, 2, 3, 3, 3, 3, 2, 2, 3, 2, 1, 1, 0};
    long n = sizeof(request) / sizeof(request[0]);
    op(mgr, request, n);
  }
  {
    long request[] = {4, 5, 1, 1};
    long n = sizeof(request) / sizeof(request[0]);
    op(mgr, request, n);
  }
  {
    long request[] = {4, 4, 4, 4, 4, 4, 0};
    long n = sizeof(request) / sizeof(request[0]);
    op(mgr, request, n);
  }
  {
    long request[] = {8, 9, 10};
    long n = sizeof(request) / sizeof(request[0]);
    op(mgr, request, n);
  }
  {
    long request[] = {11, 12, 13, 12, 11, 12, 13, 12, 11};
    long n = sizeof(request) / sizeof(request[0]);
    op(mgr, request, n);
  }
  // {
  //   long request[] = {0,1,2,3,4,5};
  //   long n = sizeof(request) / sizeof(request[0]);
  //   op(mgr, request, n);
  // }
  return 0;
}