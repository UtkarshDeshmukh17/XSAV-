#include "Event.h"

namespace xsav {

const wchar_t* ToString(FileEventType type) {
    switch (type) {
        case FileEventType::Created:            return L"CREATED";
        case FileEventType::Modified:           return L"MODIFIED";
        case FileEventType::Deleted:            return L"DELETED";
        case FileEventType::RenamedOld:         return L"RENAMED_OLD";
        case FileEventType::RenamedNew:         return L"RENAMED_NEW";
        case FileEventType::AttributesChanged:  return L"ATTRIBUTES_CHANGED";
    }
    return L"UNKNOWN";
}

} // namespace xsav
