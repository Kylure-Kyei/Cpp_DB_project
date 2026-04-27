#ifndef MY_MAP_H
#define MY_MAP_H

#include "MyVector.h"
#include "MyString.h"
#include <stdexcept>

// 定义一个简单的键值对结构
template <typename K, typename V>
struct Pair {
    K first;
    V second;
    
    Pair() : first(), second() {}
    Pair(const K& k, const V& v) : first(k), second(v) {}
};

template <typename K, typename V>
class MyMap {
private:
    MyVector<Pair<K, V>> data_; // 使用我们刚才造的轮子

public:
    // 构造函数
    MyMap() = default;

    // 获取大小
    size_t size() const {
        return data_.size();
    }

    // 判空
    bool empty() const {
        return data_.empty();
    }

    // 查找元素，返回引用
    V& at(const K& key) {
        for (size_t i = 0; i < data_.size(); ++i) {
            if (data_[i].first == key) {
                return data_[i].second;
            }
        }
        throw std::out_of_range("Key not found");
    }

    // 下标访问操作符（如果键不存在则创建）
    V& operator[](const K& key) {
        // 先尝试查找是否已存在
        for (size_t i = 0; i < data_.size(); ++i) {
            if (data_[i].first == key) {
                return data_[i].second;
            }
        }
        // 不存在，插入新元素
        data_.push_back(Pair<K, V>(key, V()));
        return data_[data_.size() - 1].second;
    }

    // 检查是否包含某个键
    bool contains(const K& key) const {
        for (size_t i = 0; i < data_.size(); ++i) {
            if (data_[i].first == key) {
                return true;
            }
        }
        return false;
    }

    // 清空
    void clear() {
        data_.clear();
    }
};

#endif // MY_MAP_H
