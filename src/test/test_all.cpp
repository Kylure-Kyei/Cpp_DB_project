#include <cstdio>
#include <cassert>
#include <cstring>
#include "../common/MyString.h"
#include "../common/MyVector.h"
#include "../common/MyMap.h"
#include "../index/BPlusTree.h"

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

    printf("\nAll core tests passed.\n");
    return 0;
}