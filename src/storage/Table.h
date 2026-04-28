#ifndef MINIDB_TABLE_H
#define MINIDB_TABLE_H

#include "Row.h"
#include "common/Logger.h"
#include "../common/MyString.h"
#include "common/Logger.h"
#include "../common/MyVector.h"
#include "common/Logger.h"
#include <cstdio>
#include "common/Logger.h"

/**
 * @brief 表元数据：列定义
 */
struct ColumnSchema {
    MyString name;      // 列名
    FieldType type;     // 数据类型
    bool nullable;      // 是否允许为空

    ColumnSchema() = default;

    ColumnSchema(const char* n, FieldType t, bool null = false)
        : name(n), type(t), nullable(null) {}
};

/**
 * @brief 表类
 * 负责管理数据文件（.dat）的读写
 */
class Table {
private:
    MyString tableName;                 // 表名
    MyVector<ColumnSchema> schema;      // 表结构
    FILE* dataFile;                     // 数据文件指针 (.dat)
    long rowCount;                      // 行数统计

    // 私有辅助函数：序列化/反序列化
    bool serializeRow(const Row& row, MyVector<char>& buffer);
    bool deserializeRow(const char* buffer, size_t size, Row& row);

public:
    Table(const char* name);
    ~Table();

    /**
     * @brief 初始化表（创建文件或打开文件）
     */
    bool init();

    /**
     * @brief 添加列定义
     */
    void addColumn(const ColumnSchema& col);

    /**
     * @brief 插入一行数据
     */
    bool insertRow(const Row& row);

    /**
     * @brief 根据行号查找数据（全表扫描基础）
     */
    bool getRow(long rowId, Row& row);

    /**
     * @brief 获取表结构信息
     */
    const MyVector<ColumnSchema>& getSchema() const;
};

#endif //MINIDB_TABLE_H
