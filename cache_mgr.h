#pragma once

#include <algorithm>
#include <iostream>
#include <list>
#include <map>
#include <set>
#include <stack>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/*(gpu_idx_vector,
    admit_cpu_idx_vector,
    admit_to_cache_idx_vector,
    evict_cache_idx_vector,
    evict_to_cpu_idx_vector
)*/
typedef std::tuple<std::vector<long>, std::vector<long>, std::vector<long>, std::vector<long>,
                   std::vector<long>>
    CacheInstruction;

struct CacheNode {
  long cpu_idx;
  long cache_idx;
  long freq = 1;
  bool masked = false;
  std::list<CacheNode*>::iterator it;
};

class CacheIndicesManager {
 public:
  CacheIndicesManager(long cache_capacity) {
    cache_capacity_ = cache_capacity;
    init_state();
  }

  CacheInstruction prepare_ids(std::vector<long> cpu_idx_vector) {
    /*
        cpu_idx_vector(input)    --cache_map-->  gpu_idx_vector
        admit_cpu_idx_vector     ----swap--->    admit_to_cache_idx_vector
        evict_to_cpu_idx_vector  <---swap---     evict_cache_idx_vector

        return (gpu_idx_vector,
                admit_cpu_idx_vector,
                admit_to_cache_idx_vector,
                evict_cache_idx_vector,
                evict_to_cpu_idx_vector
            )
    */
    std::vector<long> gpu_idx_vector, admit_cpu_idx_vector, admit_to_cache_idx_vector,
        evict_cache_idx_vector, evict_to_cpu_idx_vector;
    /* step 1. scan over cpu_idx_vector.
                mask already-cached CacheNode.
    */
    std::unordered_set<CacheNode*> masked_node;  // record for faster de-mask
    for (auto cpu_idx : cpu_idx_vector) {
      auto cache_it = cpu_cache_map_.find(cpu_idx);
      if (cache_it != cpu_cache_map_.end()) {
        masked_node.insert(&cache_it->second);
        cache_it->second.masked = true;
      }
    }
    /* step 2. cache op for each in cpu_idx_vector.
                prevent masked CacheNode from being evicted.
                mask new admit CacheNode.
                update swap vectors.
    */
    tail_node_it_ = freq_list_.begin();
    for (auto cpu_idx : cpu_idx_vector) {
      auto cache_idx = get_cache_idx(cpu_idx);
      if (cache_idx != -1) {
        gpu_idx_vector.push_back(cache_idx);
        continue;
      }
      // check available rows
      if (available_cache_idxs_.empty()) {
        // evict
        auto evict_info = evict_cache();
        evict_cache_idx_vector.push_back(std::get<0>(evict_info));
        evict_to_cpu_idx_vector.push_back(std::get<1>(evict_info));
      }
      // admit
      auto new_node_ptr = admit_cache(cpu_idx);
      cache_idx = new_node_ptr->cache_idx;
      new_node_ptr->masked = true;
      masked_node.insert(new_node_ptr);
      admit_cpu_idx_vector.push_back(cpu_idx);
      admit_to_cache_idx_vector.push_back(cache_idx);
      gpu_idx_vector.push_back(cache_idx);
    }
    /* step 3. unmask */
    for (auto node_ptr : masked_node) {
      node_ptr->masked = false;
    }
    return CacheInstruction(gpu_idx_vector, admit_cpu_idx_vector, admit_to_cache_idx_vector,
                            evict_cache_idx_vector, evict_to_cpu_idx_vector);
  }

  void init_state() {
    cpu_cache_map_.clear();
    freq_list_.clear();
    freq_entry_.clear();
    while (!available_cache_idxs_.empty()) {
      available_cache_idxs_.pop();
    }
    for (long i = cache_capacity_ - 1; i >= 0; i--) {
      available_cache_idxs_.push(i);
    }
    tail_node_it_ = freq_list_.end();
  }

 private:
  long cache_capacity_ = 0;
  // cpu_idx -> CacheNode
  std::unordered_map<long, CacheNode> /*                 */ cpu_cache_map_;
  // FreqNodes sorted by freq
  std::list<CacheNode*> /*                               */ freq_list_;
  // freq -> the last freq_list_ iterator holding that freq
  std::unordered_map<long, std::list<CacheNode*>::iterator> freq_entry_;
  //
  std::stack<long> /*                                    */ available_cache_idxs_;
  // unmasked LFU node
  std::list<CacheNode*>::iterator /*                     */ tail_node_it_;

  long get_cache_idx(long cpu_idx) {
    /*
    get cache idx from a cpu idx request.
    if cache idx is not ready, return -1.
    */
    auto cache_node_it = cpu_cache_map_.find(cpu_idx);
    if (cache_node_it == cpu_cache_map_.cend()) {
      return -1;
    }
    touch_cache(cache_node_it->second);
    return cache_node_it->second.cache_idx;
  }

  void touch_cache(CacheNode& cache_node) {
    auto old_freq = cache_node.freq;
    auto new_freq = ++cache_node.freq;
    auto freq_it = cache_node.it;
    if (freq_entry_.find(new_freq) == freq_entry_.end()) {
      // add new freq entry
      freq_entry_[new_freq] = freq_it;
    }
    auto old_freq_entry_it = freq_entry_.find(old_freq)->second;
    if (old_freq_entry_it == freq_it) {
      // redirect or delete entry
      if (old_freq_entry_it != freq_list_.begin() &&
          (*std::prev(old_freq_entry_it))->freq == old_freq) {
        freq_entry_[old_freq] = std::prev(old_freq_entry_it);
      } else {
        freq_entry_.erase(old_freq);
      }
    } else if (std::next(freq_it) != freq_list_.end() && (*std::next(freq_it))->freq < new_freq) {
      // reorder link list. move freq_it to next(freq2)
      auto const& freq_it2 = std::next(freq_entry_[(*std::next(freq_it))->freq]);
      if (freq_it == tail_node_it_){
        tail_node_it_ = std::next(freq_it);
      }
        // swap trick
        freq_list_.splice(freq_it2, freq_list_, freq_it);
    }
  }

  CacheNode* admit_cache(long cpu_idx) { /* add cpu_idx to cache new_node. return new_node_ptr */
    auto cache_idx = available_cache_idxs_.top();
    available_cache_idxs_.pop();
    freq_list_.push_front(NULL);
    cpu_cache_map_[cpu_idx] = {cpu_idx, cache_idx, 1, false, freq_list_.begin()};
    auto const& new_node_ptr = &cpu_cache_map_[cpu_idx];
    *(freq_list_.begin()) = new_node_ptr;
    if (freq_entry_.find(1) == freq_entry_.end()) {
      freq_entry_[1] = freq_list_.begin();
    }
    return new_node_ptr;
  }

  std::tuple<long, long> evict_cache() {
    /* evict tail node. return (evict_gpu_idx, evict_to_cpu_idx) */
    update_tail_node_upward();
    if (tail_node_it_ == freq_list_.end()) {
      throw std::runtime_error("Error: no enough cache row num.");
    }
    auto to_delete_freq_it = tail_node_it_++;
    auto evict_gpu_idx = (*to_delete_freq_it)->cache_idx;
    auto evict_to_cpu_idx = (*to_delete_freq_it)->cpu_idx;
    auto freq = (*to_delete_freq_it)->freq;
    cpu_cache_map_.erase(evict_to_cpu_idx);
    if (freq_entry_[freq] == to_delete_freq_it) {
      // redirect or delete entry
      if (to_delete_freq_it != freq_list_.begin() &&
          (*std::prev(to_delete_freq_it))->freq == freq) {
        freq_entry_[freq] = std::prev(to_delete_freq_it);
      } else {
        freq_entry_.erase(freq);
      }
    }
    freq_list_.erase(to_delete_freq_it);
    available_cache_idxs_.push(evict_gpu_idx);
    return std::tuple<long, long>(evict_gpu_idx, evict_to_cpu_idx);
  }

  void update_tail_node_upward() {
    while (tail_node_it_ != freq_list_.end() && (*tail_node_it_)->masked) {
      tail_node_it_++;
    }
  }
};