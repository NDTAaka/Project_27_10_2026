#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
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

constexpr const char *kFile = "sum.dat";
constexpr const char *kLock = "LTHT_Bai69";
constexpr int kStop = -9999;

void writer() {
    std::mt19937 engine(std::random_device{}());
    std::uniform_int_distribution<int> distribution(0, 150000);
    std::ofstream file(kFile, std::ios::binary | std::ios::trunc);
    if (!file)
        throw std::runtime_error("Khong mo duoc sum.dat");

    while (true) {
        const int value = distribution(engine);
        FileLock lock(kLock);
        if (value > 100000) {
            file.write(reinterpret_cast<const char *>(&kStop), sizeof(kStop));
            file.flush();
            std::cout << "Sinh " << value << "; ghi ma dung " << kStop << ".\n";
            return;
        }
        file.write(reinterpret_cast<const char *>(&value), sizeof(value));
        file.flush();
        if (!file)
            throw std::runtime_error("Loi ghi sum.dat");
        std::cout << "CT1 ghi: " << value << '\n';
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

void reader() {
    std::streamoff position = 0;
    long long sum = 0;
    while (true) {
        bool stop = false;
        {
            FileLock lock(kLock);
            if (std::filesystem::exists(kFile)) {
                std::ifstream file(kFile, std::ios::binary);
                file.seekg(position);
                int value;
                while (file.read(reinterpret_cast<char *>(&value), sizeof(value))) {
                    position = file.tellg();
                    if (value == kStop) {
                        stop = true;
                        break;
                    }
                    sum += value;
                }
            }
        }
        if (stop) {
            std::cout << "Tong cac so da doc: " << sum << '\n';
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

int main(int argc, char **argv) {
    if (argc != 2 || (std::string(argv[1]) != "CT1" && std::string(argv[1]) != "CT2")) {
        std::cerr << "Cach dung: Bai_6_9 CT1|CT2\n";
        return 1;
    }
    if (std::string(argv[1]) == "CT1")
        writer();
    else
        reader();
}
