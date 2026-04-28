#include "Table.h"
#include "../common/MyString.h"
#include "../index/DiskBPlusTree.h"
#include "../storage/PageManager.h"
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <cstdint>

Table::Table(const char* name)
    : tableName(name), dataFile(nullptr), rowCount(0),
      indexPageManager_(nullptr), index_(nullptr), primaryKeyCol_(-1) {}

Table::~Table() {
    if (dataFile) fclose(dataFile);
    delete index_;
    delete indexPageManager_;
}

bool Table::init() {
    // 1. 打开/创建数据文件
    MyString filename = tableName;
    filename += ".dat";
    dataFile = fopen(filename.c_str(), "r+b");
    if (!dataFile) {
        dataFile = fopen(filename.c_str(), "w+b");
        if (!dataFile) {
            printf("Error: Cannot create data file %s: %s\n", filename.c_str(), strerror(errno));
            return false;
        }
    }

    // 2. 打开/创建索引文件（.idx）的页管理器
    MyString idxName = tableName;
    idxName += ".idx";
    try {
        indexPageManager_ = new PageManager(idxName.c_str());
    } catch (...) {
        printf("Warning: Could not open index file %s. Index disabled.\n", idxName.c_str());
    }

    return true;
}

void Table::addColumn(const ColumnSchema& col) {
    schema.push_back(col);
}

void Table::setPrimaryKey(int colIndex) {
    if (colIndex < 0 || colIndex >= static_cast<int>(schema.size())) return;
    primaryKeyCol_ = colIndex;
    // 如果页管理器已存在，创建 B+ 树索引
    if (indexPageManager_ && !index_) {
        index_ = new DiskBPlusTree(indexPageManager_, 4); // 阶数 4
    }
}

int Table::extractPrimaryKey(const Row& row) const {
    if (primaryKeyCol_ < 0 || primaryKeyCol_ >= static_cast<int>(row.getFieldCount()))
        return -1;
    const FieldValue* fv = row.getField(primaryKeyCol_);
    if (!fv || fv->type != TYPE_INT) return -1;
    return fv->int_val;
}

bool Table::insertRow(const Row& row) {
    if (!dataFile) return false;

    MyVector<char> buffer;
    if (!serializeRow(row, buffer)) return false;

    uint32_t len = static_cast<uint32_t>(buffer.size());
    fwrite(&len, sizeof(len), 1, dataFile);
    fwrite(buffer.data(), 1, buffer.size(), dataFile);
    fflush(dataFile);

    // 更新索引：将主键值和行号（rowCount）插入 B+ 树
    if (index_ && primaryKeyCol_ >= 0) {
        int key = extractPrimaryKey(row);
        if (key != -1) {
            index_->insert(key, static_cast<int>(rowCount));
        }
    }

    rowCount++;
    return true;
}

bool Table::getRow(long rowId, Row& row) {
    if (!dataFile) return false;
    rewind(dataFile);
    for (long i = 0; i <= rowId; ++i) {
        uint32_t len;
        if (fread(&len, sizeof(len), 1, dataFile) != 1) return false;
        MyVector<char> buf;
        buf.resize(len);
        if (fread(buf.data(), 1, len, dataFile) != len) return false;
        if (i == rowId) {
            return deserializeRow(buf.data(), len, row);
        }
    }
    return false;
}

bool Table::getRowByKey(int key, Row& row) {
    if (!index_ || primaryKeyCol_ < 0) return false;
    auto result = index_->search(key);
    if (!result.has_value()) return false;
    long rowId = result.value();
    return getRow(rowId, row);
}

const MyVector<ColumnSchema>& Table::getSchema() const {
    return schema;
}

// ========== 序列化/反序列化实现（与之前相同） ==========

bool Table::serializeRow(const Row& row, MyVector<char>& buffer) {
    uint32_t fieldCount = static_cast<uint32_t>(row.getFieldCount());
    buffer.resize(sizeof(fieldCount));
    memcpy(buffer.data(), &fieldCount, sizeof(fieldCount));

    size_t offset = sizeof(fieldCount);
    for (size_t i = 0; i < row.getFieldCount(); ++i) {
        const FieldValue* val = row.getField(i);
        if (!val) continue;

        buffer.resize(offset + 1 + sizeof(uint32_t));
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

bool Table::deserializeRow(const char* buffer, size_t size, Row& row) {
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