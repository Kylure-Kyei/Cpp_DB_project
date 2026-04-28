#ifndef DISK_BPLUS_TREE_H
#define DISK_BPLUS_TREE_H

#include "../storage/PageManager.h"
#include "../common/MyVector.h"
#include <optional>
#include <cstring>

using KeyType = int;
using ValueType = int;
static constexpr uint32_t INVALID_PAGE = 0xFFFFFFFF;

// ---- 磁盘节点结构（变长内容序列化到 4KB 页） ----
struct DiskBPlusNode {
    MyVector<KeyType> keys;
    MyVector<ValueType> values;          // 仅叶子
    MyVector<uint32_t> childPages;      // 内部节点使用
    uint32_t nextPage;                  // 叶子链表
    bool isLeaf;

    DiskBPlusNode(bool leaf = true) : nextPage(INVALID_PAGE), isLeaf(leaf) {}

    // 序列化到 buffer（调用者保证 buffer 大小 >= PageManager::PAGE_SIZE）
    void serialize(char* buffer) const {
        size_t offset = 0;
        buffer[offset++] = isLeaf ? 1 : 0;
        memcpy(buffer + offset, &nextPage, sizeof(nextPage)); offset += sizeof(nextPage);

        uint32_t keyCount = static_cast<uint32_t>(keys.size());
        memcpy(buffer + offset, &keyCount, sizeof(keyCount)); offset += sizeof(keyCount);
        for (size_t i = 0; i < keyCount; ++i) {
            memcpy(buffer + offset, &keys[i], sizeof(KeyType)); offset += sizeof(KeyType);
        }

        uint32_t valCount = static_cast<uint32_t>(values.size());
        memcpy(buffer + offset, &valCount, sizeof(valCount)); offset += sizeof(valCount);
        for (size_t i = 0; i < valCount; ++i) {
            memcpy(buffer + offset, &values[i], sizeof(ValueType)); offset += sizeof(ValueType);
        }

        uint32_t childCount = static_cast<uint32_t>(childPages.size());
        memcpy(buffer + offset, &childCount, sizeof(childCount)); offset += sizeof(childCount);
        for (size_t i = 0; i < childCount; ++i) {
            memcpy(buffer + offset, &childPages[i], sizeof(uint32_t)); offset += sizeof(uint32_t);
        }
        // 余下字节保持零（调用前 memset 了缓冲区）
    }

    static DiskBPlusNode deserialize(const char* buffer) {
        DiskBPlusNode node;
        size_t offset = 0;
        node.isLeaf = (buffer[offset++] == 1);
        memcpy(&node.nextPage, buffer + offset, sizeof(node.nextPage)); offset += sizeof(node.nextPage);

        uint32_t keyCount; memcpy(&keyCount, buffer + offset, sizeof(keyCount)); offset += sizeof(keyCount);
        for (uint32_t i = 0; i < keyCount; ++i) {
            KeyType k; memcpy(&k, buffer + offset, sizeof(KeyType)); offset += sizeof(KeyType);
            node.keys.push_back(k);
        }

        uint32_t valCount; memcpy(&valCount, buffer + offset, sizeof(valCount)); offset += sizeof(valCount);
        for (uint32_t i = 0; i < valCount; ++i) {
            ValueType v; memcpy(&v, buffer + offset, sizeof(ValueType)); offset += sizeof(ValueType);
            node.values.push_back(v);
        }

        uint32_t childCount; memcpy(&childCount, buffer + offset, sizeof(childCount)); offset += sizeof(childCount);
        for (uint32_t i = 0; i < childCount; ++i) {
            uint32_t c; memcpy(&c, buffer + offset, sizeof(uint32_t)); offset += sizeof(uint32_t);
            node.childPages.push_back(c);
        }
        return node;
    }
};

// ---- 磁盘 B+ 树 ----
class DiskBPlusTree {
public:
    DiskBPlusTree(PageManager* pm, size_t order = 4)
        : order_(order), rootPage_(INVALID_PAGE), pageManager_(pm) {}

    void insert(KeyType key, ValueType value);
    std::optional<ValueType> search(KeyType key);
    MyVector<ValueType> range_query(KeyType start, KeyType end);

    uint32_t getRootPage() const { return rootPage_; }

private:
    size_t order_;
    uint32_t rootPage_;
    PageManager* pageManager_;

    // 查找键所在的叶子页号
    uint32_t findLeaf(KeyType key);

    // 分裂叶子
    void splitLeaf(uint32_t leafPage);

    // 分裂内部节点
    void splitInternal(uint32_t nodePage);

    // 将键和右子页插入父节点
    void insertIntoParent(uint32_t leftPage, KeyType key, uint32_t rightPage);

    // 查找子节点的父节点页号
    uint32_t findParent(uint32_t childPage);

    // 递归查找父节点
    uint32_t findParentRecursive(uint32_t currentPage, uint32_t targetPage);
};

// -------------------- 实现 --------------------

uint32_t DiskBPlusTree::findLeaf(KeyType key) {
    if (rootPage_ == INVALID_PAGE) return INVALID_PAGE;
    uint32_t current = rootPage_;
    while (true) {
        char buf[PageManager::PAGE_SIZE] = {0};
        pageManager_->readPage(current, buf);
        DiskBPlusNode node = DiskBPlusNode::deserialize(buf);
        if (node.isLeaf) return current;
        // 内部节点：找到 key 应进入的子页
        size_t i = 0;
        while (i < node.keys.size() && key >= node.keys[i]) ++i;
        current = node.childPages[i];
    }
}

void DiskBPlusTree::insert(KeyType key, ValueType value) {
    if (rootPage_ == INVALID_PAGE) {
        // 创建根叶子
        rootPage_ = pageManager_->allocatePage();
        DiskBPlusNode leaf(true);
        leaf.keys.push_back(key);
        leaf.values.push_back(value);
        char buf[PageManager::PAGE_SIZE] = {0};
        leaf.serialize(buf);
        pageManager_->writePage(rootPage_, buf);
        return;
    }

    uint32_t leafPage = findLeaf(key);
    char buf[PageManager::PAGE_SIZE] = {0};
    pageManager_->readPage(leafPage, buf);
    DiskBPlusNode leaf = DiskBPlusNode::deserialize(buf);

    // 检查键是否已存在
    for (size_t i = 0; i < leaf.keys.size(); ++i) {
        if (leaf.keys[i] == key) {
            leaf.values[i] = value; // 更新
            memset(buf, 0, PageManager::PAGE_SIZE);
            leaf.serialize(buf);
            pageManager_->writePage(leafPage, buf);
            return;
        }
    }

    // 插入新键值对（保持有序）
    size_t i = 0;
    while (i < leaf.keys.size() && leaf.keys[i] < key) ++i;
    leaf.keys.insert(leaf.keys.begin() + i, key);
    leaf.values.insert(leaf.values.begin() + i, value);
    memset(buf, 0, PageManager::PAGE_SIZE);
    leaf.serialize(buf);
    pageManager_->writePage(leafPage, buf);

    // 检查分裂
    if (leaf.keys.size() >= order_) {
        splitLeaf(leafPage);
    }
}

void DiskBPlusTree::splitLeaf(uint32_t leafPage) {
    char buf[PageManager::PAGE_SIZE] = {0};
    pageManager_->readPage(leafPage, buf);
    DiskBPlusNode leaf = DiskBPlusNode::deserialize(buf);

    size_t mid = leaf.keys.size() / 2;
    DiskBPlusNode newLeaf(true);
    // 移动后半部分键值对到新叶子
    for (size_t i = mid; i < leaf.keys.size(); ++i) {
        newLeaf.keys.push_back(leaf.keys[i]);
        newLeaf.values.push_back(leaf.values[i]);
    }
    // 回缩原叶子
    while (leaf.keys.size() > mid) {
        leaf.keys.pop_back();
        leaf.values.pop_back();
    }

    // 维护链表
    uint32_t newPage = pageManager_->allocatePage();
    newLeaf.nextPage = leaf.nextPage;
    leaf.nextPage = newPage;

    // 写回两个叶子
    memset(buf, 0, PageManager::PAGE_SIZE);
    leaf.serialize(buf);
    pageManager_->writePage(leafPage, buf);
    memset(buf, 0, PageManager::PAGE_SIZE);
    newLeaf.serialize(buf);
    pageManager_->writePage(newPage, buf);

    // 提升第一个键到父节点
    insertIntoParent(leafPage, newLeaf.keys[0], newPage);
}

void DiskBPlusTree::insertIntoParent(uint32_t leftPage, KeyType key, uint32_t rightPage) {
    if (leftPage == rootPage_) {
        // 创建新根
        uint32_t newRoot = pageManager_->allocatePage();
        DiskBPlusNode newRootNode(false);
        newRootNode.keys.push_back(key);
        newRootNode.childPages.push_back(leftPage);
        newRootNode.childPages.push_back(rightPage);
        char buf[PageManager::PAGE_SIZE] = {0};
        newRootNode.serialize(buf);
        pageManager_->writePage(newRoot, buf);
        rootPage_ = newRoot;
        return;
    }

    uint32_t parentPage = findParent(leftPage);
    char buf[PageManager::PAGE_SIZE] = {0};
    pageManager_->readPage(parentPage, buf);
    DiskBPlusNode parent = DiskBPlusNode::deserialize(buf);

    // 找到 leftPage 在父节点子页数组中的位置，key 插入到该位置之后
    size_t i = 0;
    while (i < parent.childPages.size() && parent.childPages[i] != leftPage) ++i;
    // 插入 key 和 rightPage
    parent.keys.insert(parent.keys.begin() + i, key);
    parent.childPages.insert(parent.childPages.begin() + i + 1, rightPage);

    memset(buf, 0, PageManager::PAGE_SIZE);
    parent.serialize(buf);
    pageManager_->writePage(parentPage, buf);

    if (parent.keys.size() >= order_) {
        splitInternal(parentPage);
    }
}

void DiskBPlusTree::splitInternal(uint32_t nodePage) {
    char buf[PageManager::PAGE_SIZE] = {0};
    pageManager_->readPage(nodePage, buf);
    DiskBPlusNode node = DiskBPlusNode::deserialize(buf);

    size_t mid = node.keys.size() / 2;
    DiskBPlusNode newNode(false);
    // 移动 mid 之后的键和子页到新节点
    for (size_t i = mid + 1; i < node.keys.size(); ++i) {
        newNode.keys.push_back(node.keys[i]);
    }
    for (size_t i = mid + 1; i < node.childPages.size(); ++i) {
        newNode.childPages.push_back(node.childPages[i]);
    }
    KeyType midKey = node.keys[mid];
    // 回缩原节点
    while (node.keys.size() > mid) node.keys.pop_back();
    while (node.childPages.size() > mid + 1) node.childPages.pop_back();

    uint32_t newPage = pageManager_->allocatePage();
    // 写回原节点
    memset(buf, 0, PageManager::PAGE_SIZE);
    node.serialize(buf);
    pageManager_->writePage(nodePage, buf);
    // 写回新节点
    memset(buf, 0, PageManager::PAGE_SIZE);
    newNode.serialize(buf);
    pageManager_->writePage(newPage, buf);

    insertIntoParent(nodePage, midKey, newPage);
}

uint32_t DiskBPlusTree::findParent(uint32_t childPage) {
    if (rootPage_ == childPage) return INVALID_PAGE;
    return findParentRecursive(rootPage_, childPage);
}

uint32_t DiskBPlusTree::findParentRecursive(uint32_t currentPage, uint32_t targetPage) {
    char buf[PageManager::PAGE_SIZE] = {0};
    pageManager_->readPage(currentPage, buf);
    DiskBPlusNode node = DiskBPlusNode::deserialize(buf);
    if (node.isLeaf) return INVALID_PAGE;
    for (size_t i = 0; i < node.childPages.size(); ++i) {
        if (node.childPages[i] == targetPage) return currentPage;
        uint32_t found = findParentRecursive(node.childPages[i], targetPage);
        if (found != INVALID_PAGE) return found;
    }
    return INVALID_PAGE;
}

std::optional<ValueType> DiskBPlusTree::search(KeyType key) {
    if (rootPage_ == INVALID_PAGE) return std::nullopt;
    uint32_t leafPage = findLeaf(key);
    char buf[PageManager::PAGE_SIZE] = {0};
    pageManager_->readPage(leafPage, buf);
    DiskBPlusNode leaf = DiskBPlusNode::deserialize(buf);
    for (size_t i = 0; i < leaf.keys.size(); ++i) {
        if (leaf.keys[i] == key) return leaf.values[i];
    }
    return std::nullopt;
}

MyVector<ValueType> DiskBPlusTree::range_query(KeyType start, KeyType end) {
    MyVector<ValueType> result;
    if (rootPage_ == INVALID_PAGE) return result;
    uint32_t leafPage = findLeaf(start);
    if (leafPage == INVALID_PAGE) return result;
    while (leafPage != INVALID_PAGE) {
        char buf[PageManager::PAGE_SIZE] = {0};
        pageManager_->readPage(leafPage, buf);
        DiskBPlusNode leaf = DiskBPlusNode::deserialize(buf);
        for (size_t i = 0; i < leaf.keys.size(); ++i) {
            if (leaf.keys[i] > end) return result;
            if (leaf.keys[i] >= start) result.push_back(leaf.values[i]);
        }
        leafPage = leaf.nextPage;
    }
    return result;
}

#endif // DISK_BPLUS_TREE_H
