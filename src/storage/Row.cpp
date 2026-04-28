#include "Row.h"
#include "common/Logger.h"

void Row::addField(const FieldValue &value) {
    fields.push_back(value);
}

size_t Row::getFieldCount() const {
    return fields.size();
}

const FieldValue *Row::getField(size_t index) const {
    if (index < fields.size()) {
        return &fields[index];
    }
    return nullptr;
}
