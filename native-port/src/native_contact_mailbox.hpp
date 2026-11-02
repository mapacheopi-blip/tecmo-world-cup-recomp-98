#pragma once
#include <array>
#include <cstdint>
#include <mutex>
#include <string>
namespace tecmo98 {
// Direct input is read by SDL's main thread; the game owns its mutable session.
class NativeContactMailbox {
 std::mutex mutex_;std::array<std::uint8_t,7> contacts_{};std::uint64_t stamp_=0;bool seen_=false;
public:
 void publish(const std::array<std::uint8_t,7>& contacts,std::uint64_t stamp){std::lock_guard lock(mutex_);contacts_=contacts;stamp_=stamp;seen_=true;}
 std::array<std::uint8_t,7> read(std::uint64_t now,bool focused){std::lock_guard lock(mutex_);if(!seen_||!focused||now<stamp_||now-stamp_>250)return {};return contacts_;}
};
class NativePacketMailbox {
 std::mutex mutex_;std::string packet_;
public:
 void publish(std::string packet){std::lock_guard lock(mutex_);packet_=std::move(packet);}
 std::string read(){std::lock_guard lock(mutex_);return packet_;}
};
}
