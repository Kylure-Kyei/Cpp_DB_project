#ifndef BPLUS_TREE_H
#define BPLUS_TREE_H

#include "MyVector.h"
#include <iostream>
#include <optional>

// B+树节点
template <typename K, typename V>
struct BPlusNode {
    MyVector<K> keys;           // 键
    MyVector<V> values;         // 值（只有叶子节点存储）
    MyVector<BPlusNode*> children; // 子节点指针
    BPlusNode* next;            // 叶子节点的链表指针
    bool is_leaf;               // 是否是叶子节点

    BPlusNode(bool leaf = true) : is_leaf(leaf), next(nullptr) {}
};

template <typename K, typename V>
class BPlusTree {
private:
    BPlusNode<K, V>* root_;
    size_t order_;  // 阶数，每个节点最多有order_个子节点

    // 查找叶子节点
    BPlusNode<K, V>* find_leaf(const K& key) {
        if (root_ == nullptr) return nullptr;
        
        BPlusNode<K, V>* node = root_;
        while (!node->is_leaf) {
            size_t i = 0;
            while (i < node->keys.size() && key >= node->keys[i]) {
                i++;
            }
            node = node->children[i];
        }
        return node;
    }

    // 分裂内部节点
    void split_internal(BPlusNode<K, V>* node) {
        size_t mid = node->keys.size() / 2;
        BPlusNode<K, V>* new_node = new BPlusNode<K, V>(false);
        
        // 移动一半的键和子节点到新节点
        for (size_t i = mid + 1; i < node->keys.size(); i++) {
            new_node->keys.push_back(node->keys[i]);
            node->keys.pop_back();
        }
        for (size_t i = mid + 1; i < node->children.size(); i++) {
            new_node->children.push_back(node->children[i]);
            node->children.pop_back();
        }
        
        // 将中间键提升到父节点
        K mid_key = node->keys[mid];
        node->keys.pop_back();
        
        if (node == root_) {
            BPlusNode<K, V>* new_root = new BPlusNode<K, V>(false);
            new_root->keys.push_back(mid_key);
            new_root->children.push_back(node);
            new_root->children.push_back(new_node);
            root_ = new_root;
        } else {
            BPlusNode<K, V>* parent = nullptr;
            // 找到父节点
            // 这里简化处理，实际应该维护parent指针
            insert_into_parent(node, mid_key, new_node);
        }
    }

    // 插入到父节点
    void insert_into_parent(BPlusNode<K, V>* left, const K& key, BPlusNode<K, V>* right) {
        BPlusNode<K, V>* parent = find_parent(left);
        if (parent == nullptr) {
            // 创建新根
            BPlusNode<K, V>* new_root = new BPlusNode<K, V>(false);
            new_root->keys.push_back(key);
            new_root->children.push_back(left);
            new_root->children.push_back(right);
            root_ = new_root;
            return;
        }
        
        // 找到插入位置
        size_t i = 0;
        while (i < parent->keys.size() && parent->keys[i] < key) {
            i++;
        }
        parent->keys.insert(parent->keys.begin() + i, key);
        parent->children.insert(parent->children.begin() + i + 1, right);
        
        // 检查是否需要分裂
        if (parent->keys.size() >= order_) {
            split_internal(parent);
        }
    }

    // 查找父节点（简化版，实际需要维护parent指针）
    BPlusNode<K, V>* find_parent(BPlusNode<K, V>* child) {
        if (root_ == child) return nullptr;
        // 这里简化处理，实际应该从根遍历查找
        return nullptr;
    }

public:
    BPlusTree(size_t order = 4) : root_(nullptr), order_(order) {}

    ~BPlusTree() {
        clear(root_);
    }

    // 递归清理
    void clear(BPlusNode<K, V>* node) {
        if (node == nullptr) return;
        if (!node->is_leaf) {
            for (size_t i = 0; i < node->children.size(); i++) {
                clear(node->children[i]);
            }
        }
        delete node;
    }

    // 插入键值对
    void insert(const K& key, const V& value) {
        if (root_ == nullptr) {
            root_ = new BPlusNode<K, V>(true);
            root_->keys.push_back(key);
            root_->values.push_back(value);
            return;
        }

        BPlusNode<K, V>* leaf = find_leaf(key);
        
        // 检查键是否已存在
        for (size_t i = 0; i < leaf->keys.size(); i++) {
            if (leaf->keys[i] == key) {
                leaf->values[i] = value; // 更新值
                return;
            }
        }
        
        // 插入新键值对
        size_t i = 0;
        while (i < leaf->keys.size() && leaf->keys[i] < key) {
            i++;
        }
        leaf->keys.insert(leaf->keys.begin() + i, key);
        leaf->values.insert(leaf->values.begin() + i, value);
        
        // 检查是否需要分裂
        if (leaf->keys.size() >= order_) {
            split_leaf(leaf);
        }
    }

    // 分裂叶子节点
    void split_leaf(BPlusNode<K, V>* leaf) {
        size_t mid = leaf->keys.size() / 2;
        BPlusNode<K, V>* new_leaf = new BPlusNode<K, V>(true);
        
        // 移动一半的键值对到新叶子
        for (size_t i = mid; i < leaf->keys.size(); i++) {
            new_leaf->keys.push_back(leaf->keys[i]);
            new_leaf->values.push_back(leaf->values[i]);
            leaf->keys.pop_back();
            leaf->values.pop_back();
        }
        
        // 更新叶子节点链表
        new_leaf->next = leaf->next;
        leaf->next = new_leaf;
        
        // 将新键提升到父节点
        insert_into_parent(leaf, new_leaf->keys[0], new_leaf);
    }

    // 查找键对应的值
    std::optional<V> search(const K& key) {
        if (root_ == nullptr) return std::nullopt;
        
        BPlusNode<K, V>* leaf = find_leaf(key);
        
        for (size_t i = 0; i < leaf->keys.size(); i++) {
            if (leaf->keys[i] == key) {
                return leaf->values[i];
            }
        }
        return std::nullopt;
    }

    // 范围查询
    MyVector<V> range_query(const K& start_key, const K& end_key) {
        MyVector<V> result;
        if (root_ == nullptr) return result;
        
        BPlusNode<K, V>* leaf = find_leaf(start_key);
        
        while (leaf != nullptr) {
            for (size_t i = 0; i < leaf->keys.size(); i++) {
                if (leaf->keys[i] >= start_key && leaf->keys[i] <= end_key) {
                    result.push_back(leaf->values[i]);
                } else if (leaf->keys[i] > end_key) {
                    return result;
                }
            }
            leaf = leaf->next;
        }
        return result;
    }
};

#endif // BPLUS_TREE_H
