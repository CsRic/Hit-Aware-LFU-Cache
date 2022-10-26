# Hit Aware LFU Cache
This lfu cache features batch input. It handles only indices and returns only index swapping instructions for other modules that actually swap data.

Already-exist cache indices will be protected/masked from being evicted. 

input_batch_size = n. Overall time complexity: O(n).