#include <chrono>
#include <filesystem>
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

static_assert(sizeof(int) == 4, "Ma ASCII duoc luu bang int 4 byte.");

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

constexpr const char *kFile = "ascii.dat";
constexpr const char *kLock = "LTHT_Bai68";
constexpr int kEnd = -1;

void writer() {
    std::cout << "Nhap mot dong ASCII: ";
    std::string text;
    std::getline(std::cin, text);
    FileLock lock(kLock);
    std::ofstream file(kFile, std::ios::binary | std::ios::trunc);
    for (unsigned char character : text) {
        if (character <= 0x7F) {
            const int code = character;
            file.write(reinterpret_cast<const char *>(&code), sizeof(code));
        }
    }
    file.write(reinterpret_cast<const char *>(&kEnd), sizeof(kEnd));
    file.flush();
    if (!file)
        throw std::runtime_error("Loi ghi ascii.dat");
}

void reader() {
    while (true) {
        {
            FileLock lock(kLock);
            if (std::filesystem::exists(kFile)) {
                std::ifstream file(kFile, std::ios::binary);
                std::string text;
                int code;
                while (file.read(reinterpret_cast<char *>(&code), sizeof(code))) {
                    if (code == kEnd) {
                        std::cout << "Ky tu doc duoc: " << text << '\n';
                        return;
                    }
                    if (code < 0 || code > 0x7F)
                        throw std::runtime_error("Ma ASCII trong file khong hop le.");
                    text.push_back(static_cast<char>(code));
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

int main(int argc, char **argv) {
    if (argc != 2 || (std::string(argv[1]) != "CT1" && std::string(argv[1]) != "CT2")) {
        std::cerr << "Cach dung: Bai_6_8 CT1|CT2\n";
        return 1;
    }
    if (std::string(argv[1]) == "CT1")
        writer();
    else
        reader();
}
