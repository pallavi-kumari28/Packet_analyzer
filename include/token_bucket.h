#pragma once
#include <chrono>
#include <cstdint>
#include <algorithm>

namespace DPI {

// Token Bucket Rate Limiter
// Used to throttle bandwidth for specific flows/apps instead of blocking them outright.
//
// Concept:
//   - The bucket holds up to `capacity_bytes` tokens (1 token = 1 byte of allowance)
//   - Tokens refill continuously at `rate_bytes_per_sec`
//   - A packet of size N can only be forwarded immediately if N tokens are available
//   - Otherwise, the caller decides: drop it, delay it, or queue it

class TokenBucket {
public:
    TokenBucket(double rate_bytes_per_sec, double capacity_bytes)
        : rate_(rate_bytes_per_sec),
          capacity_(capacity_bytes),
          tokens_(capacity_bytes),
          last_refill_(std::chrono::steady_clock::now()) {}

    // Call this before forwarding a packet.
    // Returns true if the packet can be sent immediately (tokens available).
    // Returns false if it should be delayed/dropped (not enough tokens).
    bool tryConsume(size_t packet_size_bytes) {
        refill();
        if (tokens_ >= static_cast<double>(packet_size_bytes)) {
            tokens_ -= static_cast<double>(packet_size_bytes);
            return true;
        }
        return false;
    }

    // Returns how long (in milliseconds) the caller should wait
    // before this packet would have enough tokens available.
    // Useful if you want to delay instead of drop.
    int64_t millisUntilAvailable(size_t packet_size_bytes) {
        refill();
        double deficit = static_cast<double>(packet_size_bytes) - tokens_;
        if (deficit <= 0) return 0;
        double seconds_needed = deficit / rate_;
        return static_cast<int64_t>(seconds_needed * 1000.0);
    }

    double currentTokens() const { return tokens_; }

private:
    void refill() {
        auto now = std::chrono::steady_clock::now();
        double elapsed_sec = std::chrono::duration<double>(now - last_refill_).count();
        tokens_ = std::min(capacity_, tokens_ + elapsed_sec * rate_);
        last_refill_ = now;
    }

    double rate_;             // bytes per second allowed
    double capacity_;         // max burst size in bytes
    double tokens_;           // current available tokens
    std::chrono::steady_clock::time_point last_refill_;
};

} // namespace DPI
