#include "write_ahead_log.h"
#include "../storage/rcu_store.h"
#include <fstream>

void recover(RcuStore<std::string, std::string>& store, const std::filesystem::path& dir) {
    std::ifstream f(dir / "wal.log", std::ios::binary);
    if (!f) return;  // No WAL file = fresh start

    while (f.peek() != EOF) {
        uint32_t len;
        f.read(reinterpret_cast<char*>(&len), sizeof(len));

        std::vector<char> buf(len);
        f.read(buf.data(), len);
        if (f.gcount() != len) break;  // Truncated record — stop recovery here

        auto entry = WalEntry::deserialise(buf);
        switch (entry.type) {
            case WalEntry::Type::Set:
                store.set(entry.key, entry.value, entry.ttl_ms ? std::optional{std::chrono::duration_cast<std::chrono::seconds>(*entry.ttl_ms)} : std::nullopt);
                break;
            case WalEntry::Type::Delete:
                store.del(entry.key);
                break;
        }
    }
}
