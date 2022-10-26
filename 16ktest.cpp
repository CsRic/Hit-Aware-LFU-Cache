#include <experimental/random>
#include "cache_mgr.h"
#include <chrono>

#define CacheRowNum 16384
#define RandRange 16384

using namespace std;
using namespace chrono;

int main() {
  CacheIndicesManager mgr(16384);
  long request[16384];
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
  }
  cout << "total time: " << cache_op_time / 1000 << " ms" << endl;
  cout << "average time: " << cache_op_time / 1000 / repeat << " ms" << endl;
  return 0;
}