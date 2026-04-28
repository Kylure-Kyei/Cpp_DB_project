#include <cstdio>
#include <cassert>
#include <cstring>
#include "../common/MyString.h"
#include "../common/MyVector.h"
#include "../common/MyMap.h"
#include "../index/BPlusTree.h"
#include "../storage/PageManager.h"
#include "../index/DiskBPlusTree.h"

// 辅助：比较 MyString 与 C 字符串
bool str_eq(const MyString& a, const char* b) {
    return strcmp(a.c_str(), b) == 0;
}

int main() {
    // -------- MyString 测试 --------
    {
        MyString s1("hello");
        assert(s1.size() == 5);
        assert(str_eq(s1, "hello"));

        MyString s2 = s1;
        assert(str_eq(s2, "hello"));

        MyString s3 = s1 + MyString(" world");
        assert(s3.size() == 11);
        assert(str_eq(s3, "hello world"));

        s3.clear();
        assert(s3.empty());
        assert(s3.size() == 0);

        bool caught = false;
        try { s1[10]; } catch (...) { caught = true; }
        assert(caught);
        printf("MyString tests passed.\n");
    }

    // -------- MyVector 测试 --------
    {
        MyVector<int> v;
        assert(v.empty());
        v.push_back(1);
        v.push_back(2);
        v.push_back(3);
        assert(v.size() == 3);
        assert(v[0] == 1);
        v.pop_back();
        assert(v.size() == 2);
        v.insert(v.begin() + 1, 99);
        assert(v[1] == 99);
        assert(v[2] == 2);
        MyVector<int> v2 = v;
        assert(v2.size() == 3);
        assert(v2[0] == 1);
        v.clear();
        assert(v.empty());
        printf("MyVector tests passed.\n");
    }

    // -------- MyMap 测试 --------
    {
        MyMap<int, MyString> map;
        map[1] = "one";
        map[2] = "two";
        assert(map.size() == 2);
        assert(map.contains(1));
        assert(!map.contains(3));
        assert(str_eq(map[1], "one"));
        map[1] = "ONE";
        assert(str_eq(map[1], "ONE"));
        bool caught = false;
        try { map.at(3); } catch (...) { caught = true; }
        assert(caught);
        printf("MyMap tests passed.\n");
    }

    // -------- BPlusTree 测试（无分裂，order=4，插入 <=3 条） --------
    {
        BPlusTree<int, int> tree(4);
        tree.insert(5, 50);
        tree.insert(2, 20);
        tree.insert(8, 80);

        auto v = tree.search(2);
        assert(v.has_value() && v.value() == 20);
        v = tree.search(5);
        assert(v.has_value() && v.value() == 50);
        v = tree.search(999);
        assert(!v.has_value());

        auto range = tree.range_query(1, 6);
        // 应该返回 20,50（顺序不一定保证，但这里插入时叶子顺序是2,5,8）
        bool found2 = false, found5 = false;
        for (size_t i = 0; i < range.size(); ++i) {
            if (range[i] == 20) found2 = true;
            if (range[i] == 50) found5 = true;
        }
        assert(found2 && found5);
        printf("BPlusTree tests passed.\n");
    }

        // -------- PageManager 测试 --------
    {
        const char* testFile = "test_page.idx";
        // 删除旧测试文件（如果存在）
        remove(testFile);
        PageManager pm(testFile);
        assert(pm.getPageCount() == 0);

        uint32_t p0 = pm.allocatePage();
        assert(p0 == 0);
        assert(pm.getPageCount() == 1);

        // 写入测试数据
        char writeBuf[PageManager::PAGE_SIZE];
        memset(writeBuf, 0xAB, PageManager::PAGE_SIZE);
        strcpy(writeBuf, "Hello Page");
        assert(pm.writePage(p0, writeBuf));

        // 读取并验证
        char readBuf[PageManager::PAGE_SIZE];
        assert(pm.readPage(p0, readBuf));
        assert(strcmp(readBuf, "Hello Page") == 0);

        // 分配第二页
        uint32_t p1 = pm.allocatePage();
        assert(p1 == 1);
        assert(pm.getPageCount() == 2);

        // 清理测试文件
        remove(testFile);
        printf("PageManager tests passed.\n");
    }

    // -------- DiskBPlusTree 测试（磁盘索引） --------
    {
        const char* idxFile = "test_diskbtree.idx";
        remove(idxFile);
        PageManager pm(idxFile);
        DiskBPlusTree tree(&pm, 4);   // 阶数 4

        // 插入 3 条，不触发分裂
        tree.insert(5, 50);
        tree.insert(2, 20);
        tree.insert(8, 80);
        auto res = tree.search(2);
        assert(res.has_value() && res.value() == 20);
        res = tree.search(8);
        assert(res.has_value() && res.value() == 80);
        res = tree.search(999);
        assert(!res.has_value());
        auto range = tree.range_query(1, 6);
        bool found2 = false, found5 = false;
        for (size_t i = 0; i < range.size(); ++i) {
            if (range[i] == 20) found2 = true;
            if (range[i] == 50) found5 = true;
        }
        assert(found2 && found5);

        // 插入多条触发分裂（阶数 4，插入 4 条触发叶子分裂）
        tree.insert(9, 90);
        tree.insert(1, 10);
        tree.insert(3, 30);
        tree.insert(7, 70);
        // 验证所有值
        assert(tree.search(1).value() == 10);
        assert(tree.search(9).value() == 90);
        assert(tree.search(7).value() == 70);
       // 范围查询（闭区间 [1,5]，包括键5）
        auto range2 = tree.range_query(1, 5);
        assert(range2.size() == 4); // 1,2,3,5 (值10,20,30,50)

        remove(idxFile);
        printf("DiskBPlusTree tests passed.\n");
    }

    printf("\nAll core tests passed.\n");
    return 0;
}