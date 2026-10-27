#include <chrono>
#include <fstream>
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
    bool try_lock() {
#ifdef _WIN32
        const DWORD result = WaitForSingleObject(handle_, 0);
        owned_ = result == WAIT_OBJECT_0 || result == WAIT_ABANDONED;
        return owned_;
#else
        if (flock(fd_, LOCK_EX | LOCK_NB) == 0) {
            owned_ = true;
            return true;
        }
        if (errno == EWOULDBLOCK || errno == EAGAIN)
            return false;
        throw std::runtime_error("Khong lay duoc file lock.");
#endif
    }
};

constexpr const char *kFile = "retry.dat";
constexpr const char *kLock = "LTHT_Bai65";
constexpr int kStop = -1;

void writer() {
    FileLock lock(kLock);
    lock.lock();
    std::ofstream file(kFile, std::ios::binary | std::ios::trunc);
    if (!file)
        throw std::runtime_error("Khong mo duoc retry.dat");

    for (int value = 1; value <= 20; ++value) {
        file.write(reinterpret_cast<const char *>(&value), sizeof(value));
        file.flush();
        std::cout << "CT1 ghi: " << value << '\n';
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    file.write(reinterpret_cast<const char *>(&kStop), sizeof(kStop));
    file.flush();
}

void reader() {
    std::streamoff position = 0;
    std::cout << "CT2 thu mo lai file sau moi 2 ms neu dang bi khoa.\n";
    while (true) {
        FileLock lock(kLock);
        if (!lock.try_lock()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }

        bool stop = false;
        {
            std::ifstream file(kFile, std::ios::binary);
            if (file) {
                file.seekg(position);
                int value;
                while (file.read(reinterpret_cast<char *>(&value), sizeof(value))) {
                    position = file.tellg();
                    if (value == kStop) {
                        stop = true;
                        break;
                    }
                    std::cout << "CT2 doc: " << value << '\n';
                }
            }
        }
        if (stop)
            return;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
}

int main(int argc, char **argv) {
    if (argc != 2 || (std::string(argv[1]) != "CT1" && std::string(argv[1]) != "CT2")) {
        std::cerr << "Cach dung: Bai_6_5 CT1|CT2\n";
        return 1;
    }
    if (std::string(argv[1]) == "CT1")
        writer();
    else
        reader();
}
