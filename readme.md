# Hit Aware LFU Cache
This lfu cache featuring batch input. It handles only indices and return only indices swapping instructions for other modules that actually swap data.

Already-exist cache indices will be protected/masked from being evicted. 

input_batch_size = n. Overall time complexity: O(n).