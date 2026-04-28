#include "Table.h"
#include "common/Logger.h"
#include <cerrno>
#include "common/Logger.h"
#include <cstring>
#include "common/Logger.h"
#include <cstdint>
#include "common/Logger.h"
#include <cstdio>
#include "common/Logger.h"

Table::Table(const char *name) : tableName(name), dataFile(nullptr), rowCount(0) {}

Table::~Table() {
    if (dataFile) {
        fclose(dataFile);
    }
}

bool Table::init() {
     MyString filename = tableName;
    filename += ".dat";

    // 以读写模式打开，如果不存在则创建
    dataFile = fopen(filename.c_str(), "r+b");
    if (!dataFile) {
        // 尝试以创建模式打开
        dataFile = fopen(filename.c_str(), "w+b");
        if (!dataFile) {
            printf("Error: Cannot create table file %s: %s\n", filename.c_str(), strerror(errno));
            return false;
        }
    }
    return true;
}

void Table::addColumn(const ColumnSchema &col) {
    schema.push_back(col);
}

bool Table::insertRow(const Row &row) {
    if (!dataFile) return false;

    MyVector<char> buffer;
    if (!serializeRow(row, buffer)) {
        return false;
    }

    // 写入长度前缀（简化协议）
    uint32_t len = static_cast<uint32_t>(buffer.size());
    fwrite(&len, sizeof(len), 1, dataFile);
    fwrite(buffer.data(), 1, buffer.size(), dataFile);
    fflush(dataFile); // 强制刷盘（为了简单，实际项目需优化）
    rowCount++;
    return true;
}

bool Table::getRow(long rowId, Row &row) {
    // 为了简单，这里演示全表扫描找第rowId行
    // 实际项目中这里应该是根据索引定位，或者维护行偏移表
    if (!dataFile) return false;
    rewind(dataFile);

    for (long i = 0; i <= rowId; ++i) {
        uint32_t len;
        if (fread(&len, sizeof(len), 1, dataFile) != 1) {
            return false; // 读取结束或错误
        }
        MyVector<char> buf;
        buf.resize(len);
        if (fread(buf.data(), 1, len, dataFile) != len) {
            return false;
        }
        if (i == rowId) {
            return deserializeRow(buf.data(), len, row);
        }
    }
    return false;
}

const MyVector<ColumnSchema> &Table::getSchema() const {
    return schema;
}

// --- 私有序列化实现 ---

bool Table::serializeRow(const Row &row, MyVector<char> &buffer) {
    // 简单的序列化：[字段数][类型1][值1长度/内容]...
    uint32_t fieldCount = static_cast<uint32_t>(row.getFieldCount());
    buffer.resize(sizeof(fieldCount));
    memcpy(buffer.data(), &fieldCount, sizeof(fieldCount));

    size_t offset = sizeof(fieldCount);
    for (size_t i = 0; i < row.getFieldCount(); ++i) {
        const FieldValue* val = row.getField(i);
        if (!val) continue;

        // 扩展缓冲区
        buffer.resize(offset + 1 + sizeof(uint32_t)); // type + len
        buffer[offset] = static_cast<char>(val->type);
        offset += 1;

        switch (val->type) {
            case TYPE_INT: {
                uint32_t len = sizeof(int);
                memcpy(&buffer[offset], &len, sizeof(len));
                offset += sizeof(len);
                buffer.resize(offset + len);
                memcpy(&buffer[offset], &val->int_val, len);
                offset += len;
                break;
            }
            case TYPE_FLOAT: {
                uint32_t len = sizeof(float);
                memcpy(&buffer[offset], &len, sizeof(len));
                offset += sizeof(len);
                buffer.resize(offset + len);
                memcpy(&buffer[offset], &val->float_val, len);
                offset += len;
                break;
            }
            case TYPE_STRING: {
                uint32_t str_len = static_cast<uint32_t>(val->str_val.size());
                memcpy(&buffer[offset], &str_len, sizeof(str_len));
                offset += sizeof(str_len);
                buffer.resize(offset + str_len);
                memcpy(&buffer[offset], val->str_val.c_str(), str_len);
                offset += str_len;
                break;
            }
        }
    }
    return true;
}

bool Table::deserializeRow(const char *buffer, size_t size, Row &row) {
    if (size < sizeof(uint32_t)) return false;
    uint32_t fieldCount;
    memcpy(&fieldCount, buffer, sizeof(fieldCount));

    size_t offset = sizeof(uint32_t);
    for (uint32_t i = 0; i < fieldCount && offset < size; ++i) {
        if (offset + 1 > size) break;
        FieldType type = static_cast<FieldType>(buffer[offset]);
        offset += 1;

        switch (type) {
            case TYPE_INT: {
                if (offset + sizeof(uint32_t) + sizeof(int) > size) break;
                uint32_t len;
                memcpy(&len, &buffer[offset], sizeof(len));
                offset += sizeof(len);
                int val;
                memcpy(&val, &buffer[offset], sizeof(val));
                offset += sizeof(val);
                row.addField(FieldValue(val));
                break;
            }
            case TYPE_FLOAT: {
                if (offset + sizeof(uint32_t) + sizeof(float) > size) break;
                uint32_t len;
                memcpy(&len, &buffer[offset], sizeof(len));
                offset += sizeof(len);
                float val;
                memcpy(&val, &buffer[offset], sizeof(val));
                offset += sizeof(val);
                break;
            }
            case TYPE_STRING: {
                if (offset + sizeof(uint32_t) > size) break;
                uint32_t str_len;
                memcpy(&str_len, &buffer[offset], sizeof(str_len));
                offset += sizeof(str_len);
                if (offset + str_len > size) break;
                MyString str;
                // 简单处理，假设字符串不包含\0
                char temp[256] = {0};
                memcpy(temp, &buffer[offset], str_len < 255 ? str_len : 255);
                str = temp;
                offset += str_len;
                row.addField(FieldValue(temp));
                break;
            }
            default:
                return false;
        }
    }
    return true;
}
