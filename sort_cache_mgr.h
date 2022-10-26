#pragma once

#include <algorithm>
#include <cstring>
#include <iostream>
#include <list>
#include <map>
#include <set>
#include <stack>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct cache_freq_ptr_cmp {
  bool operator()(const long* p1, const long* p2) const {
    if (*p1 == *p2) {
      return p1 < p2;
    }
    return *p1 < *p2;
  }
};

class SortCacheIndicesManager {
 public:
  SortCacheIndicesManager(long cuda_row_num = 0, long cpu_row_num = 0) {
    cuda_row_num_ = cuda_row_num;
    cpu_row_num_ = cpu_row_num;
    cache_freq_ = new long[cuda_row_num_];
    cache_cpu_match_ = new long[cuda_row_num_];
    this->init_map();
  }

  ~SortCacheIndicesManager() {
    delete[] cache_freq_;
    delete[] cache_cpu_match_;
  }

  void init_map() {
    memset(cache_freq_, 0, cuda_row_num_);
    cache_freq_set_.clear();
    for (long i = 0; i < cuda_row_num_; i++) {
      cache_freq_set_.insert(cache_freq_ + i);
    }
    memset(cache_cpu_match_, -1, cuda_row_num_);
    cpu_cache_map_.clear();
    while (!available_cache_row_stack_.empty()) {
      available_cache_row_stack_.pop();
    }
    for (long i = cuda_row_num_ - 1; i >= 0; i--) {
      available_cache_row_stack_.push(i);
    }
  }

  std::tuple<std::vector<long>, std::vector<long>, std::vector<long>, std::vector<long>,
             std::vector<long>>
  prepare_ids(std::vector<long> cpu_idx_vector) {
    /*
        cpu_idx_vector -> gpu_idx_vector
        admit_cpu_idx_vector     ---swap-->  admit_to_cache_idx_vector
        evict_to_cpu_idx_vector  <--swap--   evict_cache_idx_vector
        return (gpu_idx_vector,
                admit_cpu_idx_vector,
                admit_to_cache_idx_vector,
                evict_cache_idx_vector,
                evict_to_cpu_idx_vector
            )
    */
    std::vector<long> gpu_idx_vector;
    std::vector<long> admit_cpu_idx_vector;
    std::vector<long> admit_to_cache_idx_vector;
    std::vector<long> evict_cache_idx_vector;
    std::vector<long> evict_to_cpu_idx_vector;

    // unique op
    std::vector<long> unique_cpu_idx_vector;
    std::vector<long> unique_count_vector;
    {
      std::vector<long> cpu_idx_vector_clone(cpu_idx_vector);
      std::sort(cpu_idx_vector_clone.begin(), cpu_idx_vector_clone.end());
      for (auto cpu_idx : cpu_idx_vector_clone) {
        if (unique_cpu_idx_vector.empty() || unique_cpu_idx_vector.back() != cpu_idx) {
          unique_cpu_idx_vector.push_back(cpu_idx);
          unique_count_vector.push_back(1);
        } else {
          unique_count_vector.back() += 1;
        }
      }
      if (unique_cpu_idx_vector.size() > cuda_row_num_) {
        throw std::runtime_error("Error: no enough cache row num.");
      }
    }
    // isin op
    std::vector<long> already_cached_idx_vector;
    std::vector<long> backup_freq_vector;
    {
      auto cached_cpu_idx_iter = cpu_cache_map_.begin();
      auto incoming_cpu_idx_iter = unique_cpu_idx_vector.begin();
      while (cached_cpu_idx_iter != cpu_cache_map_.end() &&
             incoming_cpu_idx_iter != unique_cpu_idx_vector.end()) {
        if (cached_cpu_idx_iter->first == *incoming_cpu_idx_iter) {
          /*
              protect this cache_idx from being evicted
              mark corresponding freqs with -1.
          */
          already_cached_idx_vector.push_back(cached_cpu_idx_iter->second);
          backup_freq_vector.push_back(cache_freq_[cached_cpu_idx_iter->second]);
          cache_freq_[cached_cpu_idx_iter->second] = -1;
          cached_cpu_idx_iter++;
          incoming_cpu_idx_iter++;
        } else if (cached_cpu_idx_iter->first < *incoming_cpu_idx_iter) {
          cached_cpu_idx_iter++;
        } else {
          admit_cpu_idx_vector.push_back(*incoming_cpu_idx_iter);
          incoming_cpu_idx_iter++;
        }
      }
      while (incoming_cpu_idx_iter != unique_cpu_idx_vector.end()){
        admit_cpu_idx_vector.push_back(*incoming_cpu_idx_iter);
        incoming_cpu_idx_iter++;
      } 
    }
    // swap op
    {
      auto admit_cpu_idx_iter = admit_cpu_idx_vector.begin();
      /* admit_cpu_idx_vector ---swap--> admit_to_cache_idx_vector */
      // 1. check available rows
      while (!available_cache_row_stack_.empty() &&
             admit_cpu_idx_iter != admit_cpu_idx_vector.end()) {
        admit_to_cache_idx_vector.push_back(this->admit_to_cache(*admit_cpu_idx_iter++));
      }
      // 2. no enough rows, evict LFU
      auto freq_order_cache_idx_ptr_iter = cache_freq_set_.begin();
      while (admit_cpu_idx_iter != admit_cpu_idx_vector.end()) {
        while (*(*freq_order_cache_idx_ptr_iter) == -1) {
          // pass marked
          freq_order_cache_idx_ptr_iter++;
        }
        auto evict_cache_idx = *freq_order_cache_idx_ptr_iter - cache_freq_;  // ptr locate trick
        evict_cache_idx_vector.push_back(evict_cache_idx);
        evict_to_cpu_idx_vector.push_back(this->evict_from_cache(evict_cache_idx));
        admit_to_cache_idx_vector.push_back(this->admit_to_cache(*admit_cpu_idx_iter++));
        freq_order_cache_idx_ptr_iter++;
      }
    }
    // restore marked freqs
    {
      auto cache_idx_iter = already_cached_idx_vector.begin();
      auto freq_iter = backup_freq_vector.begin();
      for (; cache_idx_iter != already_cached_idx_vector.end() &&
             freq_iter != backup_freq_vector.end();
           cache_idx_iter++, freq_iter++) {
        cache_freq_[*cache_idx_iter] = *freq_iter;
      }
    }
    // update freqs
    /* we don't update freqs during eviction because those marked freqs(-1) misguide sorting */
    {
      for (auto cache_idx : admit_to_cache_idx_vector) {
        cache_freq_[cache_idx] = 0;
      }
      auto incoming_cpu_idx_iter = unique_cpu_idx_vector.begin();
      auto unique_count_iter = unique_count_vector.begin();
      for (; incoming_cpu_idx_iter != unique_cpu_idx_vector.end() &&
             unique_count_iter != unique_count_vector.end();
           incoming_cpu_idx_iter++, unique_count_iter++) {
        this->update_freq(cpu_cache_map_.find(*incoming_cpu_idx_iter)->second, *unique_count_iter);
      }
    }

    for (auto cpu_idx : cpu_idx_vector) {
      gpu_idx_vector.push_back(cpu_cache_map_.find(cpu_idx)->second);
    }
    return std::tuple<std::vector<long>, std::vector<long>, std::vector<long>, std::vector<long>,
                      std::vector<long>>(gpu_idx_vector, admit_cpu_idx_vector,
                                         admit_to_cache_idx_vector, evict_cache_idx_vector,
                                         evict_to_cpu_idx_vector);
  }

 private:
  long cuda_row_num_;
  long cpu_row_num_;
  long* cache_freq_;  // gpu idx -> use freq
  std::set<long*, cache_freq_ptr_cmp>
      cache_freq_set_;  // set of cache_freq ptrs. sort by ptr target(freq).

  long* cache_cpu_match_;               // gpu idx -> cpu idx
  std::map<long, long> cpu_cache_map_;  // keys: cpu indices that are cached;
                                        // values: corresponding gpu indices
  std::stack<long> available_cache_row_stack_;

  long locate_on_cache(long cpu_idx) {
    auto cache_idx = cpu_cache_map_.find(cpu_idx);
    if (cache_idx != cpu_cache_map_.end()) {
      return cache_idx->second;
    } else {
      return -1;
    }
  }

  long admit_to_cache(long cpu_idx) {
    auto cache_idx = this->draw_available_cache();
    cache_cpu_match_[cache_idx] = cpu_idx;
    cpu_cache_map_.insert(std::pair<long, long>(cpu_idx, cache_idx));
    return cache_idx;
  }

  long evict_from_cache(long cache_idx) {
    if (cache_cpu_match_[cache_idx] == -1) {
      return -1;
    }
    auto cpu_idx = cache_cpu_match_[cache_idx];
    cache_cpu_match_[cache_idx] = -1;
    cpu_cache_map_.erase(cpu_idx);
    this->give_available_cache(cache_idx);
    return cpu_idx;
  }

  void update_freq(long cache_idx, long count) {
    // erase and reinsert ptr to maintain set order
    cache_freq_set_.erase(cache_freq_ + cache_idx);
    cache_freq_[cache_idx] += count;
    cache_freq_set_.insert(cache_freq_ + cache_idx);
  }

  void set_freq(long cache_idx, long count) {
    cache_freq_set_.erase(cache_freq_ + cache_idx);
    cache_freq_[cache_idx] = count;
    cache_freq_set_.insert(cache_freq_ + cache_idx);
  }

  long draw_available_cache() {
    if (available_cache_row_stack_.empty()) {
      throw std::runtime_error("Error: no enough cache row num.");
    }
    auto cache_idx = available_cache_row_stack_.top();
    available_cache_row_stack_.pop();
    return cache_idx;
  }

  void give_available_cache(long cache_idx) { available_cache_row_stack_.push(cache_idx); }
};