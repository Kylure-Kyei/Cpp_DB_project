#ifndef MY_VECTOR_H
#define MY_VECTOR_H

#include <iostream>
#include <stdexcept>

template <typename T>
class MyVector {
private:
    T* data_;       // 指向动态数组的指针
    size_t size_;   // 当前元素个数
    size_t capacity_; // 当前总容量

public:
    // 1. 构造函数
    MyVector() : data_(nullptr), size_(0), capacity_(0) {}

    // 2. 析构函数
    ~MyVector();

    // 3. 拷贝构造函数
    MyVector(const MyVector& other);

    // 4. 赋值运算符
    MyVector& operator=(const MyVector& other);

    // 5. 添加元素
    void push_back(const T& value);

    // 6. 获取大小
    size_t size() const;

    // 7. 判断是否为空
    bool empty() const;

    // 8. 获取容量
    size_t capacity() const;

    // 9. 下标访问
    T& operator[](size_t index);
    const T& operator[](size_t index) const;

    // 10. 清空
    void clear();

    // 11. 移除最后一个
    void pop_back();
};

// 下面是具体实现

template <typename T>
MyVector<T>::~MyVector() {
    delete[] data_;
}

template <typename T>
MyVector<T>::MyVector(const MyVector& other) 
    : data_(new T[other.capacity_]), size_(other.size_), capacity_(other.capacity_) {
    for (size_t i = 0; i < size_; ++i) {
        data_[i] = other.data_[i];
    }
}
template <typename T>
MyVector<T>& MyVector<T>::operator=(const MyVector& other) {
    if (this != &other) {
        delete[] data_;
        size_ = other.size_;
        capacity_ = other.capacity_;
        data_ = new T[capacity_];
        for (size_t i = 0; i < size_; ++i) {
            data_[i] = other.data_[i];
        }
    }
    return *this;
}

template <typename T>
void MyVector<T>::push_back(const T& value) {
    if (size_ >= capacity_) {
        // 扩容策略：如果为空则分配4个，否则翻倍
        size_t new_capacity = (capacity_ == 0) ? 4 : capacity_ * 2;
        T* new_data = new T[new_capacity];
        
        // 拷贝旧数据
        for (size_t i = 0; i < size_; ++i) {
            new_data[i] = data_[i];
        }
        
        // 释放旧内存
        delete[] data_;
        
        data_ = new_data;
        capacity_ = new_capacity;
    }
    data_[size_++] = value;
}
template <typename T>
size_t MyVector<T>::size() const {
    return size_;
}

template <typename T>
bool MyVector<T>::empty() const {
    return size_ == 0;
}

template <typename T>
size_t MyVector<T>::capacity() const {
    return capacity_;
}

template <typename T>
T& MyVector<T>::operator[](size_t index) {
    if (index >= size_) {
        throw std::out_of_range("Index out of range");
    }
    return data_[index];
}

template <typename T>
const T& MyVector<T>::operator[](size_t index) const {
    if (index >= size_) {
        throw std::out_of_range("Index out of range");
    }
    return data_[index];
}

template <typename T>
void MyVector<T>::clear() {
    size_ = 0;
}

template <typename T>
void MyVector<T>::pop_back() {
    if (size_ > 0) {
        --size_;
    }
}

#endif // MY_VECTOR_H
