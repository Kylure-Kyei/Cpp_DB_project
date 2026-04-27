#ifndef MY_STRING_H
#define MY_STRING_H

#include <iostream>
#include <cstring>
#include <stdexcept>

class MyString {
private:
    char* data_;
    size_t length_;

public:
    // 1. 构造函数
    MyString(const char* str = "");

    // 2. 析构函数
    ~MyString();

    // 3. 拷贝构造函数
    MyString(const MyString& other);

    // 4. 赋值运算符
    MyString& operator=(const MyString& other);

    // 5. 字符串拼接
    MyString operator+(const MyString& other) const;

    // 6. 获取长度
    size_t size() const;

    // 7. 下标访问
    char& operator[](size_t index);
    const char& operator[](size_t index) const;

    // 8. 转换为 C 风格字符串
    const char* c_str() const;

    // 9. 判空
    bool empty() const;

    // 10. 清空
    void clear();
};

// --- 实现部分 ---

// 构造函数
MyString::MyString(const char* str) {
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
MyString::~MyString() {
    delete[] data_;
}

// 拷贝构造函数
MyString::MyString(const MyString& other) {
    length_ = other.length_;
    data_ = new char[length_ + 1];
    strcpy(data_, other.data_);
}

// 赋值运算符
MyString& MyString::operator=(const MyString& other) {
    if (this != &other) {
        delete[] data_;
        length_ = other.length_;
        data_ = new char[length_ + 1];
        strcpy(data_, other.data_);
    }
    return *this;
}

// 字符串拼接
MyString MyString::operator+(const MyString& other) const {
    size_t new_len = length_ + other.length_;
    char* temp = new char[new_len + 1];
    
    strcpy(temp, data_);
    strcat(temp, other.data_);
    
    MyString result(temp);
    delete[] temp; // 释放临时内存
    return result;
}

// 获取长度
size_t MyString::size() const {
    return length_;
}

// 下标访问
char& MyString::operator[](size_t index) {
    if (index >= length_) {
        throw std::out_of_range("Index out of range");
    }
    return data_[index];
}

const char& MyString::operator[](size_t index) const {
    if (index >= length_) {
        throw std::out_of_range("Index out of range");
    }
    return data_[index];
}

// 转换为 C 风格字符串
const char* MyString::c_str() const {
    return data_;
}

// 判空
bool MyString::empty() const {
    return length_ == 0;
}

// 清空
void MyString::clear() {
    delete[] data_;
    length_ = 0;
    data_ = new char[1];
    data_[0] = '\0';
}

#endif // MY_STRING_H
