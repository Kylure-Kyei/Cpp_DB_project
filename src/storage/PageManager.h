#ifndef MINIDB_PAGE_MANAGER_H
#define MINIDB_PAGE_MANAGER_H

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <stdexcept>

class PageManager {
public:
    static constexpr size_t PAGE_SIZE = 4096;

    explicit PageManager(const char* filename) : file_(nullptr), pageCount_(0) {
        file_ = fopen(filename, "r+b");   // 尝试读写打开
        if (!file_) {
            // 文件不存在，创建
            file_ = fopen(filename, "w+b");
            if (!file_) {
                throw std::runtime_error("PageManager: cannot create file");
            }
            pageCount_ = 0;
        } else {
            // 读取现有文件大小，计算页数
            fseek(file_, 0, SEEK_END);
            long size = ftell(file_);
            pageCount_ = static_cast<uint32_t>(size / PAGE_SIZE);
            if (size % PAGE_SIZE != 0) {
                // 文件损坏？截断到整页
                fclose(file_);
                throw std::runtime_error("PageManager: file size not multiple of page size");
            }
        }
    }

    ~PageManager() {
        if (file_) {
            fclose(file_);
        }
    }

    // 分配新页，返回页号
    uint32_t allocatePage() {
        // 页号从0开始递增
        uint32_t newPage = pageCount_++;
        // 扩展文件大小：写入全零页
        fseek(file_, 0, SEEK_END);
        char zero[PAGE_SIZE] = {0};
        fwrite(zero, 1, PAGE_SIZE, file_);
        fflush(file_);
        return newPage;
    }

    bool readPage(uint32_t pageNo, char* buffer) {
        if (pageNo >= pageCount_) return false;
        fseek(file_, static_cast<long>(pageNo) * PAGE_SIZE, SEEK_SET);
        return fread(buffer, 1, PAGE_SIZE, file_) == PAGE_SIZE;
    }

    bool writePage(uint32_t pageNo, const char* buffer) {
        if (pageNo >= pageCount_) return false;
        fseek(file_, static_cast<long>(pageNo) * PAGE_SIZE, SEEK_SET);
        return fwrite(buffer, 1, PAGE_SIZE, file_) == PAGE_SIZE;
    }

    uint32_t getPageCount() const { return pageCount_; }

private:
    FILE* file_;
    uint32_t pageCount_;
};

#endif // MINIDB_PAGE_MANAGER_H
