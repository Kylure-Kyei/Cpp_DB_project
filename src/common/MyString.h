#ifndef MY_STRING_H
#define MY_STRING_H

#include <iostream>
#include "common/Logger.h"
#include <cstring>
#include "common/Logger.h"
#include <stdexcept>
#include "common/Logger.h"

class MyString {
private:
    char* data_;
    size_t length_;

public:
    // 1. 构造函数
    inline MyString(const char* str = "");

    // 2. 析构函数
    inline ~MyString();

    // 3. 拷贝构造函数
    inline MyString(const MyString& other);

    // 4. 赋值运算符
    inline MyString& operator=(const MyString& other);

    // 5. 字符串拼接
    inline MyString operator+(const MyString& other) const;

    // 6. 获取长度
    inline size_t size() const;

    // 7. 下标访问
    inline char& operator[](size_t index);
    inline const char& operator[](size_t index) const;

    // 8. 转换为 C 风格字符串
    inline const char* c_str() const;

    // 9. 判空
    inline bool empty() const;

    // 10. 清空
    inline void clear();

    // 11. 拼接赋值（你原文件里声明了这个，但没实现，我也补上 inline）
    inline MyString& operator+=(const char* str);
};

// --- 实现部分 (保持原样，inline 声明已生效) ---

// 构造函数
inline MyString::MyString(const char* str) {
    if (str == nullptr) {
        length_ = 0;
        data_ = new char[1];
        data_[0] = '\0';
    } else {
        length_ = strlen(str);
        data_ = new char[length_ + 1]; // +1 为了 '\0'
        strcpy(data_, str);
    }
}

// 析构函数
inline MyString::~MyString() {
    delete[] data_;
}

// 拷贝构造函数
inline MyString::MyString(const MyString& other) {
    length_ = other.length_;
    data_ = new char[length_ + 1];
    strcpy(data_, other.data_);
}

// 赋值运算符
inline MyString& MyString::operator=(const MyString& other) {
    if (this != &other) {
        delete[] data_;
        length_ = other.length_;
        data_ = new char[length_ + 1];
        strcpy(data_, other.data_);
    }
    return *this;
}

// 字符串拼接
inline MyString MyString::operator+(const MyString& other) const {
    size_t new_len = length_ + other.length_;
    char* temp = new char[new_len + 1];
    strcpy(temp, data_);
    strcat(temp, other.data_);
    MyString result(temp);
    delete[] temp; // 释放临时内存
    return result;
}

// 获取长度
inline size_t MyString::size() const {
    return length_;
}

// 下标访问
inline char& MyString::operator[](size_t index) {
    if (index >= length_) {
        throw std::out_of_range("Index out of range");
    }
    return data_[index];
}

inline const char& MyString::operator[](size_t index) const {
    if (index >= length_) {
        throw std::out_of_range("Index out of range");
    }
    return data_[index];
}

// 转换为 C 风格字符串
inline const char* MyString::c_str() const {
    return data_;
}

// 判空
inline bool MyString::empty() const {
    return length_ == 0;
}

// 清空
inline void MyString::clear() {
    delete[] data_;
    length_ = 0;
    data_ = new char[1];
    data_[0] = '\0';
}

// 拼接赋值
inline MyString& MyString::operator+=(const char* str) {
    if (str == nullptr) return *this;
    
    size_t new_len = length_ + strlen(str);
    char* new_data = new char[new_len + 1];
    
    strcpy(new_data, data_);
    strcat(new_data, str);
    
    delete[] data_;
    data_ = new_data;
    length_ = new_len;
    
    return *this;
}

#endif // MY_STRING_H