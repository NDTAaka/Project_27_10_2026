#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

static_assert(sizeof(int) == 4, "Bai nay yeu cau int 4 byte.");
static_assert(sizeof(std::int64_t) == 8, "Timestamp can 8 byte.");

class FileLock {
#ifdef _WIN32
    HANDLE handle_ = nullptr;
#else
    int fd_ = -1;
#endif
    bool owned_ = false;

  public:
    explicit FileLock(const std::string &name) {
#ifdef _WIN32
        const std::string mutexName = "Local\\" + name;
        handle_ = CreateMutexA(nullptr, FALSE, mutexName.c_str());
        if (!handle_)
            throw std::runtime_error("Khong tao duoc named mutex.");
#else
        const std::string lockPath = "." + name + ".lock";
        fd_ = open(lockPath.c_str(), O_CREAT | O_RDWR, 0666);
        if (fd_ == -1)
            throw std::runtime_error("Khong mo duoc file khoa.");
#endif
        lock();
    }
    FileLock(const FileLock &) = delete;
    FileLock &operator=(const FileLock &) = delete;
    ~FileLock() {
        unlock();
#ifdef _WIN32
        if (handle_)
            CloseHandle(handle_);
#else
        if (fd_ != -1)
            close(fd_);
#endif
    }
    void lock() {
#ifdef _WIN32
        const DWORD result = WaitForSingleObject(handle_, INFINITE);
        if (result != WAIT_OBJECT_0 && result != WAIT_ABANDONED)
            throw std::runtime_error("Khong lay duoc named mutex.");
#else
        while (flock(fd_, LOCK_EX) == -1)
            if (errno != EINTR)
                throw std::runtime_error("Khong lay duoc file lock.");
#endif
        owned_ = true;
    }
    void unlock() {
        if (!owned_)
            return;
#ifdef _WIN32
        ReleaseMutex(handle_);
#else
        flock(fd_, LOCK_UN);
#endif
        owned_ = false;
    }
};

constexpr const char *kFile = "latency.dat";
constexpr const char *kLock = "LTHT_Bai610";
constexpr std::int64_t kStop = -1;
constexpr int kSamples = 20;

std::int64_t timestamp_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

void writer() {
    std::ofstream file(kFile, std::ios::binary | std::ios::trunc);
    if (!file)
        throw std::runtime_error("Khong mo duoc latency.dat");

    for (int value = 1; value <= kSamples; ++value) {
        {
            FileLock lock(kLock);
            const std::int64_t sentAt = timestamp_ns();
            file.write(reinterpret_cast<const char *>(&sentAt), sizeof(sentAt));
            file.write(reinterpret_cast<const char *>(&value), sizeof(value));
            file.flush();
            if (!file)
                throw std::runtime_error("Loi ghi latency.dat");
        }
        std::cout << "CT1 ghi mau " << value << '\n';
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    FileLock lock(kLock);
    const std::int64_t stop = kStop;
    const int unused = 0;
    file.write(reinterpret_cast<const char *>(&stop), sizeof(stop));
    file.write(reinterpret_cast<const char *>(&unused), sizeof(unused));
    file.flush();
}

void reader() {
    std::streamoff position = 0;
    while (true) {
        bool stop = false;
        {
            FileLock lock(kLock);
            if (std::filesystem::exists(kFile)) {
                std::ifstream file(kFile, std::ios::binary);
                file.seekg(position);
                std::int64_t sentAt;
                int value;
                while (file.read(reinterpret_cast<char *>(&sentAt), sizeof(sentAt)) &&
                       file.read(reinterpret_cast<char *>(&value), sizeof(value))) {
                    position = file.tellg();
                    if (sentAt == kStop) {
                        stop = true;
                        break;
                    }
                    const double latencyMs = (timestamp_ns() - sentAt) / 1'000'000.0;
                    std::cout << "Mau " << value << ": do tre " << std::fixed
                              << std::setprecision(3) << latencyMs << " ms\n";
                }
            }
        }
        if (stop)
            return;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

int main(int argc, char **argv) {
    if (argc != 2 || (std::string(argv[1]) != "CT1" && std::string(argv[1]) != "CT2")) {
        std::cerr << "Cach dung: Bai_6_10 CT1|CT2\n";
        return 1;
    }
    if (std::string(argv[1]) == "CT1")
        writer();
    else
        reader();
}
