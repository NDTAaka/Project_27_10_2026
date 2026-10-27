#include <chrono>
#include <condition_variable>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <random>
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

constexpr const char *kFile = "data.bin";
constexpr int kCount = 20;
std::mutex stateMutex;
std::condition_variable stateChanged;
bool ready = false;
bool acknowledged = false;

void writerTask() {
    std::mt19937 engine(std::random_device{}());
    std::uniform_int_distribution<int> distribution(1, 10000);
    for (int i = 0; i < kCount; ++i) {
        const int value = distribution(engine);
        {
            FileLock lock("LTHT_Bai63");
            std::ofstream file(kFile, std::ios::binary | std::ios::trunc);
            file.write(reinterpret_cast<const char *>(&value), sizeof(value));
            file.flush();
            if (!file)
                throw std::runtime_error("Loi ghi data.bin");
        }
        std::cout << "Thread 1 ghi: " << value << '\n';
        {
            std::unique_lock<std::mutex> lock(stateMutex);
            ready = true;
            stateChanged.notify_all();
            stateChanged.wait(lock, [] { return acknowledged; });
            acknowledged = false;
            ready = false;
        }
    }
}

void readerTask() {
    for (int i = 0; i < kCount; ++i) {
        {
            std::unique_lock<std::mutex> lock(stateMutex);
            stateChanged.wait(lock, [] { return ready; });
        }
        int value;
        {
            FileLock lock("LTHT_Bai63");
            std::ifstream file(kFile, std::ios::binary);
            if (!file.read(reinterpret_cast<char *>(&value), sizeof(value)))
                throw std::runtime_error("Loi doc data.bin");
        }
        if (value % 2 == 0)
            std::cout << "Thread 2 so chan: " << value << " (0x" << std::hex << std::uppercase
                      << value << std::dec << ")\n";
        {
            std::lock_guard<std::mutex> lock(stateMutex);
            acknowledged = true;
        }
        stateChanged.notify_all();
    }
}

int main() {
    std::thread thread1(writerTask);
    std::thread thread2(readerTask);
    thread1.join();
    thread2.join();
}
