#include "BandBus.h"
#include <chrono>
#include <mutex>
#include <new>
#include <thread>

#if defined(_WIN32)
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
#else
 #include <fcntl.h>
 #include <sys/mman.h>
 #include <sys/stat.h>
 #include <unistd.h>
#endif

namespace band
{
namespace
{
constexpr uint32_t kMagic = 0x54544231;   // "TTB1"
constexpr uint32_t kVersion = 1;

struct Shared
{
    std::atomic<uint32_t> magic;
    std::atomic<uint32_t> version;
    std::atomic<uint32_t> leaderSeq;
    Leader leader;
    std::atomic<uint32_t> drumSeq;
    Drums drums;
    std::atomic<int64_t> seen[NumWho];
};

Shared* g = nullptr;
std::once_flag once;

void open()
{
    void* mem = nullptr;
    const size_t size = sizeof (Shared);
#if defined(_WIN32)
    HANDLE h = CreateFileMappingW (INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, (DWORD) size, L"Local\\TeaTowelBandBus1");
    if (h != nullptr) mem = MapViewOfFile (h, FILE_MAP_ALL_ACCESS, 0, 0, size);   // the handle stays open for the life of the plugin
#else
    const int fd = shm_open ("/teatowel_bandbus1", O_CREAT | O_RDWR, 0600);
    if (fd >= 0)
    {
        struct stat st {};
        if (fstat (fd, &st) == 0 && (size_t) st.st_size < size) (void) ftruncate (fd, (off_t) size);
        void* p = mmap (nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (p != MAP_FAILED) mem = p;
        close (fd);
    }
#endif
    if (mem == nullptr)
    {
        static Shared local;   // no shared memory: plugins in this one module can still talk
        mem = &local;
    }
    auto* s = static_cast<Shared*> (mem);
    // fresh memory is all zeros; the first one in sets it up
    uint32_t expect = 0;
    if (s->magic.compare_exchange_strong (expect, 0xFFFFFFFFu))
    {
        s->leaderSeq = 0; s->drumSeq = 0;
        new (&s->leader) Leader();
        new (&s->drums) Drums();
        for (auto& w : s->seen) w = 0;
        s->version = kVersion;
        s->magic = kMagic;
    }
    else
    {
        for (int i = 0; i < 200 && s->magic.load() != kMagic; ++i) std::this_thread::yield();
        if (s->magic.load() != kMagic || s->version.load() != kVersion) return;   // someone else's layout: stay alone
    }
    g = s;
}

Shared* bus() { std::call_once (once, open); return g; }

template <typename T>
void seqWrite (std::atomic<uint32_t>& seq, T& dst, const T& src)
{
    const uint32_t s = seq.load (std::memory_order_relaxed);
    seq.store (s + 1, std::memory_order_relaxed);
    std::atomic_thread_fence (std::memory_order_release);
    std::memcpy ((void*) &dst, &src, sizeof (T));
    std::atomic_thread_fence (std::memory_order_release);
    seq.store (s + 2, std::memory_order_release);
}

template <typename T>
bool seqRead (const std::atomic<uint32_t>& seq, const T& src, T& dst)
{
    for (int tries = 0; tries < 64; ++tries)
    {
        const uint32_t a = seq.load (std::memory_order_acquire);
        if (a & 1u) continue;
        std::memcpy (&dst, (const void*) &src, sizeof (T));
        std::atomic_thread_fence (std::memory_order_acquire);
        if (seq.load (std::memory_order_relaxed) == a) return true;
    }
    return false;
}
} // namespace

int64_t nowMs()
{
    using namespace std::chrono;
    return duration_cast<milliseconds> (system_clock::now().time_since_epoch()).count();
}

bool available() { return bus() != nullptr; }

void writeLeader (const Leader& l)
{
    if (auto* s = bus())
    {
        Leader c = l; c.heartbeatMs = nowMs();
        seqWrite (s->leaderSeq, s->leader, c);
        s->seen[WBandleader] = c.heartbeatMs;
    }
}

bool readLeader (Leader& out)
{
    auto* s = bus();
    if (s == nullptr) return false;
    Leader l;
    if (! seqRead (s->leaderSeq, s->leader, l)) return false;
    if (nowMs() - l.heartbeatMs > 1500) return false;
    out = l;
    return true;
}

void publishKicks (int64_t bar, uint16_t mask)
{
    auto* s = bus();
    if (s == nullptr) return;
    Drums d;
    seqRead (s->drumSeq, s->drums, d);
    const int slot = (int) (((bar % 4) + 4) % 4);
    d.bar[slot] = bar; d.kick[slot] = mask; d.heartbeatMs = nowMs();
    seqWrite (s->drumSeq, s->drums, d);
}

bool readKicks (int64_t bar, uint16_t& mask)
{
    auto* s = bus();
    if (s == nullptr) return false;
    Drums d;
    if (! seqRead (s->drumSeq, s->drums, d) || nowMs() - d.heartbeatMs > 3000) return false;
    const int slot = (int) (((bar % 4) + 4) % 4);
    if (d.bar[slot] != bar) return false;
    mask = d.kick[slot];
    return true;
}

void markPresent (Who w) { if (auto* s = bus()) s->seen[w].store (nowMs(), std::memory_order_relaxed); }
bool isPresent (Who w) { auto* s = bus(); return s != nullptr && nowMs() - s->seen[w].load (std::memory_order_relaxed) < 1500; }
} // namespace band
