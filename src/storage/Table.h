#ifndef MINIDB_TABLE_H
#define MINIDB_TABLE_H

#include "Row.h"
#include "../common/MyString.h"
#include "../common/MyVector.h"
#include <cstdio>

// 前置声明，避免循环包含
class PageManager;
class DiskBPlusTree;

struct ColumnSchema {
    MyString name;
    FieldType type;
    bool nullable;

    ColumnSchema() = default;
    ColumnSchema(const char* n, FieldType t, bool null = false)
        : name(n), type(t), nullable(null) {}
};

class Table {
private:
    MyString tableName;
    MyVector<ColumnSchema> schema;
    FILE* dataFile;
    long rowCount;

    // 索引相关成员
    PageManager* indexPageManager_;      // 管理 .idx 文件
    DiskBPlusTree* index_;               // 主键 B+ 树索引
    int primaryKeyCol_;                  // 主键列索引（-1 表示无索引）

    // 序列化辅助
    bool serializeRow(const Row& row, MyVector<char>& buffer);
    bool deserializeRow(const char* buffer, size_t size, Row& row);

    // 从行中提取主键值（假设主键列为 INT 类型）
    int extractPrimaryKey(const Row& row) const;

public:
    Table(const char* name);
    ~Table();

    bool init();                           // 同时打开数据文件和索引文件
    void addColumn(const ColumnSchema& col);
    void setPrimaryKey(int colIndex);      // 设置主键列，并激活索引

    bool insertRow(const Row& row);        // 插入时自动更新索引
    bool getRow(long rowId, Row& row);     // 按行号读取
    bool getRowByKey(int key, Row& row);   // 按主键值走索引读取

    const MyVector<ColumnSchema>& getSchema() const;
};

#endif //MINIDB_TABLE_H