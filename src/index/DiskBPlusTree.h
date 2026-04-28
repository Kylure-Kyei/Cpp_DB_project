#ifndef DISK_BPLUS_TREE_H
#define DISK_BPLUS_TREE_H

#include "../storage/PageManager.h"
#include "../common/MyVector.h"
#include <optional>
#include <cstring>

using KeyType = int;
using ValueType = int;
static constexpr uint32_t INVALID_PAGE = 0xFFFFFFFF;

// ---- 磁盘节点结构 ----
struct DiskBPlusNode {
    MyVector<KeyType> keys;
    MyVector<ValueType> values;
    MyVector<uint32_t> childPages;
    uint32_t nextPage;
    bool isLeaf;

    DiskBPlusNode(bool leaf = true) : nextPage(INVALID_PAGE), isLeaf(leaf) {}

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

// ---- 磁盘 B+ 树（含删除） ----
class DiskBPlusTree {
public:
    DiskBPlusTree(PageManager* pm, size_t order = 4) : order_(order), rootPage_(INVALID_PAGE), pageManager_(pm) {}

    void insert(KeyType key, ValueType value);
    bool remove(KeyType key);  // 删除
    std::optional<ValueType> search(KeyType key);
    MyVector<ValueType> range_query(KeyType start, KeyType end);

    uint32_t getRootPage() const { return rootPage_; }

private:
    size_t order_;
    uint32_t rootPage_;
    PageManager* pageManager_;

    // 节点最小键数（根据阶数计算）
    size_t minKeys() const { return (order_ - 1) / 2; }       // 叶子节点
    size_t minInternalKeys() const { return minKeys(); }      // 内部节点也可一样，为避免太复杂

    uint32_t findLeaf(KeyType key);
    void splitLeaf(uint32_t leafPage);
    void splitInternal(uint32_t nodePage);
    void insertIntoParent(uint32_t leftPage, KeyType key, uint32_t rightPage);
    uint32_t findParent(uint32_t childPage);
    uint32_t findParentRecursive(uint32_t currentPage, uint32_t targetPage);

    // 删除辅助
    void removeFromLeaf(uint32_t leafPage, KeyType key);
    void borrowFromLeftSibling(uint32_t nodePage, uint32_t parentPage, size_t childIndex);
    void borrowFromRightSibling(uint32_t nodePage, uint32_t parentPage, size_t childIndex);
    void mergeWithSibling(uint32_t nodePage, uint32_t parentPage, size_t childIndex);
    void adjustRoot();  // 如果根节点变空，调整根
};

// -------------------- 实现 --------------------

// 插入相关（未变）
inline uint32_t DiskBPlusTree::findLeaf(KeyType key) {
    if (rootPage_ == INVALID_PAGE) return INVALID_PAGE;
    uint32_t current = rootPage_;
    while (true) {
        char buf[PageManager::PAGE_SIZE] = {0};
        pageManager_->readPage(current, buf);
        DiskBPlusNode node = DiskBPlusNode::deserialize(buf);
        if (node.isLeaf) return current;
        size_t i = 0;
        while (i < node.keys.size() && key >= node.keys[i]) ++i;
        current = node.childPages[i];
    }
}

inline void DiskBPlusTree::insert(KeyType key, ValueType value) {
    if (rootPage_ == INVALID_PAGE) {
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

    for (size_t i = 0; i < leaf.keys.size(); ++i) {
        if (leaf.keys[i] == key) {
            leaf.values[i] = value;
            memset(buf, 0, PageManager::PAGE_SIZE);
            leaf.serialize(buf);
            pageManager_->writePage(leafPage, buf);
            return;
        }
    }

    size_t i = 0;
    while (i < leaf.keys.size() && leaf.keys[i] < key) ++i;
    leaf.keys.insert(leaf.keys.begin() + i, key);
    leaf.values.insert(leaf.values.begin() + i, value);
    memset(buf, 0, PageManager::PAGE_SIZE);
    leaf.serialize(buf);
    pageManager_->writePage(leafPage, buf);

    if (leaf.keys.size() >= order_) {
        splitLeaf(leafPage);
    }
}

inline void DiskBPlusTree::splitLeaf(uint32_t leafPage) {
    char buf[PageManager::PAGE_SIZE] = {0};
    pageManager_->readPage(leafPage, buf);
    DiskBPlusNode leaf = DiskBPlusNode::deserialize(buf);
    size_t mid = leaf.keys.size() / 2;
    DiskBPlusNode newLeaf(true);
    for (size_t i = mid; i < leaf.keys.size(); ++i) {
        newLeaf.keys.push_back(leaf.keys[i]);
        newLeaf.values.push_back(leaf.values[i]);
    }
    while (leaf.keys.size() > mid) {
        leaf.keys.pop_back();
        leaf.values.pop_back();
    }
    uint32_t newPage = pageManager_->allocatePage();
    newLeaf.nextPage = leaf.nextPage;
    leaf.nextPage = newPage;

    memset(buf, 0, PageManager::PAGE_SIZE);
    leaf.serialize(buf);
    pageManager_->writePage(leafPage, buf);
    memset(buf, 0, PageManager::PAGE_SIZE);
    newLeaf.serialize(buf);
    pageManager_->writePage(newPage, buf);

    insertIntoParent(leafPage, newLeaf.keys[0], newPage);
}

inline void DiskBPlusTree::insertIntoParent(uint32_t leftPage, KeyType key, uint32_t rightPage) {
    if (leftPage == rootPage_) {
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

    size_t i = 0;
    while (i < parent.childPages.size() && parent.childPages[i] != leftPage) ++i;
    parent.keys.insert(parent.keys.begin() + i, key);
    parent.childPages.insert(parent.childPages.begin() + i + 1, rightPage);

    memset(buf, 0, PageManager::PAGE_SIZE);
    parent.serialize(buf);
    pageManager_->writePage(parentPage, buf);

    if (parent.keys.size() >= order_) {
        splitInternal(parentPage);
    }
}

inline void DiskBPlusTree::splitInternal(uint32_t nodePage) {
    char buf[PageManager::PAGE_SIZE] = {0};
    pageManager_->readPage(nodePage, buf);
    DiskBPlusNode node = DiskBPlusNode::deserialize(buf);

    size_t mid = node.keys.size() / 2;
    DiskBPlusNode newNode(false);
    for (size_t i = mid + 1; i < node.keys.size(); ++i) {
        newNode.keys.push_back(node.keys[i]);
    }
    for (size_t i = mid + 1; i < node.childPages.size(); ++i) {
        newNode.childPages.push_back(node.childPages[i]);
    }
    KeyType midKey = node.keys[mid];
    while (node.keys.size() > mid) node.keys.pop_back();
    while (node.childPages.size() > mid + 1) node.childPages.pop_back();

    uint32_t newPage = pageManager_->allocatePage();
    memset(buf, 0, PageManager::PAGE_SIZE);
    node.serialize(buf);
    pageManager_->writePage(nodePage, buf);
    memset(buf, 0, PageManager::PAGE_SIZE);
    newNode.serialize(buf);
    pageManager_->writePage(newPage, buf);

    insertIntoParent(nodePage, midKey, newPage);
}

inline uint32_t DiskBPlusTree::findParent(uint32_t childPage) {
    if (rootPage_ == childPage) return INVALID_PAGE;
    return findParentRecursive(rootPage_, childPage);
}

inline uint32_t DiskBPlusTree::findParentRecursive(uint32_t currentPage, uint32_t targetPage) {
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

inline std::optional<ValueType> DiskBPlusTree::search(KeyType key) {
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

inline MyVector<ValueType> DiskBPlusTree::range_query(KeyType start, KeyType end) {
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

// ============ 删除相关实现 ============

// 从叶子节点中删除键
inline void DiskBPlusTree::removeFromLeaf(uint32_t leafPage, KeyType key) {
    char buf[PageManager::PAGE_SIZE] = {0};
    pageManager_->readPage(leafPage, buf);
    DiskBPlusNode leaf = DiskBPlusNode::deserialize(buf);
    
    size_t idx = 0;
    while (idx < leaf.keys.size() && leaf.keys[idx] != key) idx++;
    if (idx == leaf.keys.size()) return; // 没找到，不应发生
    
        // 手动删除 keys[idx]：向前移动后续元素
    for (size_t i = idx; i < leaf.keys.size() - 1; ++i) {
        leaf.keys[i] = leaf.keys[i + 1];
    }
    leaf.keys.pop_back();
    // 同样处理 values
    for (size_t i = idx; i < leaf.values.size() - 1; ++i) {
        leaf.values[i] = leaf.values[i + 1];
    }
    leaf.values.pop_back();

    memset(buf, 0, PageManager::PAGE_SIZE);
    leaf.serialize(buf);
    pageManager_->writePage(leafPage, buf);
}

// 删除主流程
inline bool DiskBPlusTree::remove(KeyType key) {
    if (rootPage_ == INVALID_PAGE) return false;
    uint32_t leafPage = findLeaf(key);
    if (leafPage == INVALID_PAGE) return false;
    
    // 检查键是否存在
    bool found = false;
    {
        char buf[PageManager::PAGE_SIZE] = {0};
        pageManager_->readPage(leafPage, buf);
        DiskBPlusNode leaf = DiskBPlusNode::deserialize(buf);
        for (size_t i = 0; i < leaf.keys.size(); ++i) {
            if (leaf.keys[i] == key) { found = true; break; }
        }
    }
    if (!found) return false;

    // 从叶子删除
    removeFromLeaf(leafPage, key);

    // 处理可能的不平衡
    // 如果根节点是叶子，且为空，则清空树
    if (leafPage == rootPage_) {
        char buf[PageManager::PAGE_SIZE] = {0};
        pageManager_->readPage(rootPage_, buf);
        DiskBPlusNode root = DiskBPlusNode::deserialize(buf);
        if (root.keys.empty()) {
            rootPage_ = INVALID_PAGE;
        }
        return true;
    }

    // 检查叶子节点是否需要合并或借用
    uint32_t parentPage = findParent(leafPage);
    if (parentPage == INVALID_PAGE) return true; // 安全保护

    while (true) {
        char buf[PageManager::PAGE_SIZE] = {0};
        pageManager_->readPage(parentPage, buf);
        DiskBPlusNode parent = DiskBPlusNode::deserialize(buf);

        // 找到该叶子在父节点中的索引
        size_t childIndex = 0;
        while (childIndex < parent.childPages.size() && parent.childPages[childIndex] != leafPage) childIndex++;
        if (childIndex == parent.childPages.size()) break; // 未找到

        // 读取叶子节点
        pageManager_->readPage(leafPage, buf);
        DiskBPlusNode leaf = DiskBPlusNode::deserialize(buf);

        // 检查是否需要借或合并
        if (leaf.keys.size() >= minKeys()) break; // 节点键数足够，平衡结束

        // 先尝试向左兄弟借
        if (childIndex > 0) {
            uint32_t leftSiblingPage = parent.childPages[childIndex - 1];
            pageManager_->readPage(leftSiblingPage, buf);
            DiskBPlusNode leftSibling = DiskBPlusNode::deserialize(buf);
            if (leftSibling.keys.size() > minKeys()) {
                borrowFromLeftSibling(leafPage, parentPage, childIndex);
                break; // 借到之后，通常就平衡了
            }
        }
        // 再尝试向右兄弟借
        if (childIndex < parent.childPages.size() - 1) {
            uint32_t rightSiblingPage = parent.childPages[childIndex + 1];
            pageManager_->readPage(rightSiblingPage, buf);
            DiskBPlusNode rightSibling = DiskBPlusNode::deserialize(buf);
            if (rightSibling.keys.size() > minKeys()) {
                borrowFromRightSibling(leafPage, parentPage, childIndex);
                break;
            }
        }
        // 两个兄弟都不够借，执行合并
        // 优先和左兄弟合并，如果没有左兄弟则和右兄弟合并
        if (childIndex > 0) {
            mergeWithSibling(leafPage, parentPage, childIndex); // 合并到左兄弟
        } else {
            mergeWithSibling(leafPage, parentPage, childIndex); // 与右兄弟合并（此时 childIndex==0）
        }

        // 合并后父节点可能变少，向上递归调整
        leafPage = parentPage;
        parentPage = findParent(parentPage);
        if (parentPage == INVALID_PAGE) {
            adjustRoot();
            break;
        }
    }
    return true;
}

// 向左兄弟借一个键值对
inline void DiskBPlusTree::borrowFromLeftSibling(uint32_t nodePage, uint32_t parentPage, size_t childIndex) {
    // nodePage 是当前节点，childIndex 是它在父节点 childPages 中的位置
    // 左兄弟一定是叶子（当前是叶子，兄弟也是叶子），对于内部节点暂不实现（树目前仅支持叶子删除）
    char buf[PageManager::PAGE_SIZE] = {0};
    
    // 读取左兄弟
    uint32_t leftPage = 0;
    {
        pageManager_->readPage(parentPage, buf);
        DiskBPlusNode parent = DiskBPlusNode::deserialize(buf);
        leftPage = parent.childPages[childIndex - 1];
    }
    pageManager_->readPage(leftPage, buf);
    DiskBPlusNode leftSibling = DiskBPlusNode::deserialize(buf);
    pageManager_->readPage(nodePage, buf);
    DiskBPlusNode node = DiskBPlusNode::deserialize(buf);

    // 将左兄弟的最后一个键值移到当前节点最前面
    KeyType moveKey = leftSibling.keys.back();
    ValueType moveVal = leftSibling.values.back();
    node.keys.insert(node.keys.begin(), moveKey);
    node.values.insert(node.values.begin(), moveVal);
    leftSibling.keys.pop_back();
    leftSibling.values.pop_back();

    // 更新父节点中的分隔键（即当前节点第一个键要更新？）
    // 对于叶子节点，父节点中的分隔键通常是当前节点的第一个键，我们保持父节点不变？
    // 实际上从左边借，当前节点的第一个键改变了，需要更新父节点中对应分隔键为该新的第一个键
    {
        pageManager_->readPage(parentPage, buf);
        DiskBPlusNode parent = DiskBPlusNode::deserialize(buf);
        parent.keys[childIndex - 1] = node.keys[0]; // 分隔键是左兄弟最大键（即 new first key of node? 让我们思考）
        // 在 B+ 树中，父节点中索引 i 的键实际上存储的是子节点 i 的最后一个键（或其右子树的第一个键？）
        // 常见实现：对于叶子，父节点的 key[i] 是子节点 i 的最大键（或者子节点 i 的最大键？）
        // 我们目前在 insertIntoParent 中是将提升键（新叶子的第一个键）插入父节点。
        // 那么父节点中 key[i] 表示子节点 i+1 的第一个键。在分裂叶子时，newLeaf.keys[0] 插入父节点，这意味着父键将左子树和右子树分开：左子树所有键 < parent.key, 右子树 >= parent.key。
        // 当从左兄弟借键时，我们将左兄弟的最后一个键移到当前节点，左兄弟的最后一个键不再是其最大键，而当前节点原来的第一个键不再是其最小键。
        // 父节点中分隔键应该是左兄弟的最大键（即左兄弟移走后的新最大键），还是当前节点移动后新的最小键？
        // 按照 B+ 树定义，父键通常存储的是左子树的最大键。如果从左兄弟借走最后一个键，左兄弟的最大键变小了，因此父节点中分隔键需要更新为左兄弟新的最大键。
        // 所以我们更新 parent.keys[childIndex - 1] = leftSibling.keys.back()（左兄弟移走后的最大键）
        parent.keys[childIndex - 1] = leftSibling.keys.back();
        memset(buf, 0, PageManager::PAGE_SIZE);
        parent.serialize(buf);
        pageManager_->writePage(parentPage, buf);
    }

    // 写回节点
    memset(buf, 0, PageManager::PAGE_SIZE);
    leftSibling.serialize(buf);
    pageManager_->writePage(leftPage, buf);
    memset(buf, 0, PageManager::PAGE_SIZE);
    node.serialize(buf);
    pageManager_->writePage(nodePage, buf);
}

// 向右兄弟借一个键值对
inline void DiskBPlusTree::borrowFromRightSibling(uint32_t nodePage, uint32_t parentPage, size_t childIndex) {
    char buf[PageManager::PAGE_SIZE] = {0};
    uint32_t rightPage = 0;
    {
        pageManager_->readPage(parentPage, buf);
        DiskBPlusNode parent = DiskBPlusNode::deserialize(buf);
        rightPage = parent.childPages[childIndex + 1];
    }
    pageManager_->readPage(rightPage, buf);
    DiskBPlusNode rightSibling = DiskBPlusNode::deserialize(buf);
    pageManager_->readPage(nodePage, buf);
    DiskBPlusNode node = DiskBPlusNode::deserialize(buf);

    // 将右兄弟的第一个键值移到当前节点最后
    KeyType moveKey = rightSibling.keys[0];
    ValueType moveVal = rightSibling.values[0];
    node.keys.push_back(moveKey);
    node.values.push_back(moveVal);
    // 从右兄弟中移除第一个
    for (size_t i = 0; i < rightSibling.keys.size() - 1; ++i) {
        rightSibling.keys[i] = rightSibling.keys[i + 1];
    }
    rightSibling.keys.pop_back();
    for (size_t i = 0; i < rightSibling.values.size() - 1; ++i) {
        rightSibling.values[i] = rightSibling.values[i + 1];
    }
    rightSibling.values.pop_back();

    // 更新父节点分隔键（childIndex 处的键），它代表当前节点（node）的最大键，移动后当前节点多了一个键，最大键变了，可能是新增的那个键？但新增的键来自右兄弟的第一个键，比当前节点原最大键大，所以当前节点最大键就是新增键。父键应更新为 node.keys.back()。
    {
        pageManager_->readPage(parentPage, buf);
        DiskBPlusNode parent = DiskBPlusNode::deserialize(buf);
        parent.keys[childIndex] = node.keys.back();  // 当前节点的最大键
        memset(buf, 0, PageManager::PAGE_SIZE);
        parent.serialize(buf);
        pageManager_->writePage(parentPage, buf);
    }

    memset(buf, 0, PageManager::PAGE_SIZE);
    rightSibling.serialize(buf);
    pageManager_->writePage(rightPage, buf);
    memset(buf, 0, PageManager::PAGE_SIZE);
    node.serialize(buf);
    pageManager_->writePage(nodePage, buf);
}

// 与左兄弟合并（如果 childIndex > 0，否则与右兄弟合并，这个函数根据参数调用）
inline void DiskBPlusTree::mergeWithSibling(uint32_t nodePage, uint32_t parentPage, size_t childIndex) {
    char buf[PageManager::PAGE_SIZE] = {0};
    pageManager_->readPage(parentPage, buf);
    DiskBPlusNode parent = DiskBPlusNode::deserialize(buf);
    
    uint32_t siblingPage;
    bool isLeftMerge = (childIndex > 0);
    if (isLeftMerge) {
        siblingPage = parent.childPages[childIndex - 1];
    } else {
        siblingPage = parent.childPages[childIndex + 1];
    }
    
    // 读取两个节点
    pageManager_->readPage(nodePage, buf);
    DiskBPlusNode node = DiskBPlusNode::deserialize(buf);
    memset(buf, 0, PageManager::PAGE_SIZE);
    pageManager_->readPage(siblingPage, buf);
    DiskBPlusNode sibling = DiskBPlusNode::deserialize(buf);
    
    // 合并：将 node 的所有键值对加到 sibling 的尾部
    if (isLeftMerge) {
        // 将 node 的内容合并到左兄弟
        for (size_t i = 0; i < node.keys.size(); ++i) {
            sibling.keys.push_back(node.keys[i]);
            sibling.values.push_back(node.values[i]);
        }
        // 更新叶子链表：左兄弟的 next 指向 node 的 next
        sibling.nextPage = node.nextPage;
        // 写回左兄弟
        memset(buf, 0, PageManager::PAGE_SIZE);
        sibling.serialize(buf);
        pageManager_->writePage(siblingPage, buf);
        // 从父节点中删除分隔键和右子页（node）
        // MyVector 无 remove，手动移除
        for (size_t i = childIndex - 1; i < parent.keys.size() - 1; ++i) parent.keys[i] = parent.keys[i + 1];
        parent.keys.pop_back();
        // 移除 childPages 中的 node 页（索引 childIndex）
        for (size_t i = childIndex; i < parent.childPages.size() - 1; ++i) parent.childPages[i] = parent.childPages[i + 1];
        parent.childPages.pop_back();
    } else {
        // 将右兄弟的内容合并到 node（当前节点），然后删除右兄弟
        for (size_t i = 0; i < sibling.keys.size(); ++i) {
            node.keys.push_back(sibling.keys[i]);
            node.values.push_back(sibling.values[i]);
        }
        node.nextPage = sibling.nextPage;
        // 写回 node
        memset(buf, 0, PageManager::PAGE_SIZE);
        node.serialize(buf);
        pageManager_->writePage(nodePage, buf);
        // 从父节点中删除分隔键和右兄弟页 
        for (size_t i = childIndex; i < parent.keys.size() - 1; ++i) parent.keys[i] = parent.keys[i + 1];
        parent.keys.pop_back();
        for (size_t i = childIndex + 1; i < parent.childPages.size() - 1; ++i) parent.childPages[i] = parent.childPages[i + 1];
        parent.childPages.pop_back();
    }

    // 写回父节点
    memset(buf, 0, PageManager::PAGE_SIZE);
    parent.serialize(buf);
    pageManager_->writePage(parentPage, buf);

    // 如果父节点是根且变空，则调整根
    if (parentPage == rootPage_ && parent.keys.empty()) {
        if (isLeftMerge) rootPage_ = siblingPage;
        else rootPage_ = nodePage;
    }
}

// 调整根（若根节点为空且非叶子，用子节点作为新根）
inline void DiskBPlusTree::adjustRoot() {
    if (rootPage_ == INVALID_PAGE) return;
    char buf[PageManager::PAGE_SIZE] = {0};
    pageManager_->readPage(rootPage_, buf);
    DiskBPlusNode root = DiskBPlusNode::deserialize(buf);
    if (!root.isLeaf && root.keys.empty()) {
        rootPage_ = root.childPages[0];
    }
}
#endif