#ifndef MINIDB_ROW_H
#define MINIDB_ROW_H

#include "../common/MyString.h"
#include "common/Logger.h"
#include "../common/MyVector.h"
#include "common/Logger.h"

/**
 * @brief 数据行中的字段类型枚举
 */
enum FieldType {
    TYPE_INT,
    TYPE_STRING,
    TYPE_FLOAT
};

/**
 * @brief 字段值联合体（简化版，实际项目中可能需要更复杂的Variant）
 */
struct FieldValue {
    FieldType type;
    union {
        int int_val;
        float float_val;
    };
    MyString str_val; // string单独处理，因为包含构造/析构

    FieldValue() : type(TYPE_INT), int_val(0) {}
    explicit FieldValue(int v) : type(TYPE_INT), int_val(v) {}
    explicit FieldValue(float v) : type(TYPE_FLOAT), float_val(v) {}
    explicit FieldValue(const char* v) : type(TYPE_STRING) { str_val = v; }
};

/**
 * @brief 数据行类
 * 用于表示表中的一行数据
 */
class Row {
private:
    MyVector<FieldValue> fields; // 字段集合

public:
    Row() = default;
    ~Row() = default;

    /**
     * @brief 添加一个字段值
     */
    void addField(const FieldValue& value);

    /**
     * @brief 获取字段数量
     */
    size_t getFieldCount() const;

    /**
     * @brief 获取指定索引的字段值（用于调试/序列化）
     */
    const FieldValue* getField(size_t index) const;
};

#endif //MINIDB_ROW_H
