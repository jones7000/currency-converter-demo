#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <fmt/format.h>
#include <list>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>

#include <Poco/Logger.h>

#include "cache/ICache.h"

namespace currency::cache {

// Combined LRU (size-based) + TTL (time-based) cache.
//
// Data structure: std::unordered_map for O(1) key lookup + std::list for
// O(1) move-to-front / evict-oldest, the textbook LRU pairing.
//
// Locking: a single std::mutex guards the whole structure. Every read also
// mutates state (it moves the accessed entry to the front of the list), so
// a "read" is never actually read-only -- it needs the exclusive lock
// anyway, which is why this does not use a std::shared_mutex. Sharding by
// key hash is the concrete next step if profiling ever showed contention.
//
// Expiry: checked lazily on access, plus a periodic background sweep so
// entries that are written once and never read again are still reclaimed.
// An expired entry is removed outright, never served stale.
template <typename Value>
class TimeAndSizeBoundedCache : public ICache<Value> {
public:
    TimeAndSizeBoundedCache(std::string name, std::chrono::minutes maxAge, std::size_t maxElements)
        : name_(std::move(name)),
          maxAge_(maxAge),
          maxElements_(maxElements),
          logger_(Poco::Logger::get("cache." + name_)) {
        if (maxElements_ == 0) {
            throw std::invalid_argument("max_elements must be > 0");
        }
        sweeper_ = std::thread([this] { sweepLoop(); });
    }

    ~TimeAndSizeBoundedCache() override {
        stopping_.store(true);
        sweeperCv_.notify_all();
        if (sweeper_.joinable()) {
            sweeper_.join();
        }
    }

    TimeAndSizeBoundedCache(const TimeAndSizeBoundedCache&) = delete;
    TimeAndSizeBoundedCache& operator=(const TimeAndSizeBoundedCache&) = delete;

    std::optional<Value> get(const std::string& key) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = index_.find(key);
        if (it == index_.end()) {
            return std::nullopt;
        }
        if (isExpired(it->second->insertedAt)) {
            if (logger_.debug()) {
                logger_.debug(fmt::format("cache[{}] expired on access, key={}", name_, key));
            }
            entries_.erase(it->second);
            index_.erase(it);
            return std::nullopt;
        }
        entries_.splice(entries_.begin(), entries_, it->second);
        return it->second->value;
    }

    void put(const std::string& key, Value value) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = index_.find(key);
        if (it != index_.end()) {
            entries_.erase(it->second);
            index_.erase(it);
        }

        entries_.push_front(Entry{key, std::move(value), std::chrono::steady_clock::now()});
        index_[key] = entries_.begin();

        if (entries_.size() > maxElements_) {
            const auto& oldest = entries_.back();
            if (logger_.debug()) {
                logger_.debug(fmt::format("cache[{}] capacity evicted, key={}", name_, oldest.key));
            }
            index_.erase(oldest.key);
            entries_.pop_back();
        }
    }

private:
    struct Entry {
        std::string key;
        Value value;
        std::chrono::steady_clock::time_point insertedAt;
    };

    bool isExpired(std::chrono::steady_clock::time_point insertedAt) const {
        return std::chrono::steady_clock::now() - insertedAt > maxAge_;
    }

    void sweepLoop() {
        // Wake up at roughly half the TTL (never less than a minute, so a
        // very small max_age used in local testing doesn't spin) and
        // reclaim anything that expired without being re-read.
        auto interval = maxAge_ / 2 > std::chrono::minutes(1) ? maxAge_ / 2 : std::chrono::minutes(1);
        std::unique_lock<std::mutex> lock(sweeperMutex_);
        while (!stopping_.load()) {
            sweeperCv_.wait_for(lock, interval, [this] { return stopping_.load(); });
            if (stopping_.load()) {
                break;
            }
            sweepExpired();
        }
    }

    void sweepExpired() {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto it = entries_.begin(); it != entries_.end();) {
            if (isExpired(it->insertedAt)) {
                if (logger_.debug()) {
                    logger_.debug(fmt::format("cache[{}] ttl sweep removed, key={}", name_, it->key));
                }
                index_.erase(it->key);
                it = entries_.erase(it);
            } else {
                ++it;
            }
        }
    }

    std::string name_;
    std::chrono::minutes maxAge_;
    std::size_t maxElements_;
    Poco::Logger& logger_;

    std::mutex mutex_;
    std::list<Entry> entries_;
    std::unordered_map<std::string, typename std::list<Entry>::iterator> index_;

    std::thread sweeper_;
    std::atomic<bool> stopping_{false};
    std::mutex sweeperMutex_;
    std::condition_variable sweeperCv_;
};

} // namespace currency::cache
