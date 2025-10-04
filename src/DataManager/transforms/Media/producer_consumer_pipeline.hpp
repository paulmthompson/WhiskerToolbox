#ifndef PRODUCER_CONSUMER_PIPELINE_HPP
#define PRODUCER_CONSUMER_PIPELINE_HPP

#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <functional>
#include <optional>
#include <iostream>

/**
 * @brief A thread-safe, blocking queue for producer-consumer patterns.
 *
 * This queue has a maximum size to prevent excessive memory usage.
 * It is designed to be closed to signal the end of production, allowing
 * consumers to terminate gracefully.
 *
 * @tparam T The type of items to be stored in the queue.
 */
template <typename T>
class BlockingQueue {
public:
    /**
     * @brief Constructs a blocking queue with a specified maximum size.
     * @param max_size The maximum number of items the queue can hold.
     */
    explicit BlockingQueue(size_t max_size) : max_size_(max_size) {}

    /**
     * @brief Pushes an item into the queue, blocking if the queue is full.
     * @param item The item to push, moved into the queue.
     * @return True if the item was pushed successfully, false if the queue was closed.
     */
    bool push(T&& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        not_full_.wait(lock, [this] { return queue_.size() < max_size_ || closed_; });

        if (closed_) {
            return false;
        }

        queue_.push(std::move(item));
        not_empty_.notify_one();
        return true;
    }

    /**
     * @brief Pops an item from the queue, blocking if the queue is empty.
     * @param item A reference to store the popped item.
     * @return True if an item was popped successfully, false if the queue is empty and has been closed.
     */
    bool pop(T& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        not_empty_.wait(lock, [this] { return !queue_.empty() || closed_; });

        if (queue_.empty() && closed_) {
            return false;
        }

        item = std::move(queue_.front());
        queue_.pop();
        not_full_.notify_one();
        return true;
    }

    /**
     * @brief Closes the queue, preventing any further pushes.
     *
     * This will unblock any waiting producer or consumer threads.
     */
    void close() {
        std::unique_lock<std::mutex> lock(mutex_);
        closed_ = true;
        not_full_.notify_all();
        not_empty_.notify_all();
    }

private:
    std::queue<T> queue_;
    std::mutex mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
    size_t max_size_;
    bool closed_ = false;
};

/**
 * @brief Runs a generic single-producer, single-consumer pipeline.
 *
 * This function orchestrates the creation of a producer thread and a consumer
 * loop on the calling thread. It handles data batching and progress reporting.
 *
 * @tparam ItemType The type of data items being processed.
 * @param queue_size The maximum number of items to buffer between producer and consumer.
 * @param total_items The total number of items to be produced.
 * @param producer_func A function that produces a single item given an index.
 * It should return std::nullopt if production for that item fails.
 * @param consumer_func A function that consumes a batch of items.
 * @param batch_size The desired number of items to group into a batch for the consumer.
 * @param progress_callback A function to report progress (0-100).
 */
template <typename ItemType>
void run_pipeline(
    size_t queue_size,
    size_t total_items,
    std::function<std::optional<ItemType>(size_t)> producer_func,
    std::function<void(std::vector<ItemType>)> consumer_func,
    size_t batch_size,
    std::function<void(int)> progress_callback)
{
    if (total_items == 0) {
        if (progress_callback) progress_callback(100);
        return;
    }

    BlockingQueue<ItemType> queue(queue_size);
    std::atomic<size_t> processed_count{0};

    // --- Producer Thread ---
    std::thread producer_thread([&]() {
        for (size_t i = 0; i < total_items; ++i) {
            std::optional<ItemType> item = producer_func(i);
            if (item) {
                if (!queue.push(std::move(*item))) {
                    // Queue was closed prematurely, stop producing.
                    std::cerr << "Pipeline producer unable to push to closed queue." << std::endl;
                    break;
                }
            }
        }
        // Signal that production is finished
        queue.close();
    });

    // --- Consumer Logic (on calling thread) ---
    std::vector<ItemType> batch;
    batch.reserve(batch_size);
    ItemType item;

    while (queue.pop(item)) {
        batch.push_back(std::move(item));
        if (batch.size() >= batch_size) {
            consumer_func(batch);
            processed_count += batch.size();
            if (progress_callback) {
                progress_callback(static_cast<int>(100.0 * processed_count / total_items));
            }
            batch.clear();
        }
    }

    // Process any remaining items in the last partial batch
    if (!batch.empty()) {
        consumer_func(batch);
        processed_count += batch.size();
        if (progress_callback) {
            progress_callback(static_cast<int>(100.0 * processed_count / total_items));
        }
        batch.clear();
    }
    
    producer_thread.join();

    if (progress_callback) progress_callback(100);
}

#endif // PRODUCER_CONSUMER_PIPELINE_HPP
