// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "vsitags.hpp"
#include "vsistruct.hpp"
#include <list>

namespace slideio
{
    namespace vsi
    {
        class TagInfo
        {
        public:
            TagInfo(TagInfo&& other) noexcept
                : tag(other.tag),
                  fieldType(other.fieldType),
                  valueType(other.valueType),
                  extendedType(other.extendedType),
                  secondTag(other.secondTag),
                  extended(other.extended),
                  dataSize(other.dataSize),
                  name(std::move(other.name)),
                  children(std::move(other.children)),
                  value(std::move(other.value)) {
            }

            TagInfo& operator=(TagInfo&& other) noexcept {
                if (this == &other)
                    return *this;
                tag = other.tag;
                fieldType = other.fieldType;
                valueType = other.valueType;
                extendedType = other.extendedType;
                secondTag = other.secondTag;
                extended = other.extended;
                dataSize = other.dataSize;
                name = std::move(other.name);
                children = std::move(other.children);
                value = std::move(other.value);
                return *this;
            }
            ~TagInfo() = default;

            typedef std::list<TagInfo> TagInfos;
            typedef TagInfos::const_iterator const_iterator;
            typedef TagInfos::iterator iterator;
            TagInfo() = default;
            TagInfo(const TagInfo& other) {
                copy(other);
            }
            TagInfo& operator=(const TagInfo& other) {
                copy(other);
                return *this;
            }
            iterator end() { return children.end(); }
            iterator begin() { return children.begin(); }
            const_iterator end() const { return children.end(); }
            const_iterator begin() const { return children.begin(); }
            const_iterator find(int tag) {
                return std::find_if(children.begin(), children.end(), [tag](const TagInfo& info) {
                    return info.tag == tag;
                    });
            }
            void addChild(const TagInfo& info) {
                children.push_back(info);
            }
            void setValue(const std::string& srcValue) {
                this->value = srcValue;
            }
            void copy(const TagInfo& other) {
                               tag = other.tag;
                fieldType = other.fieldType;
                valueType = other.valueType;
                extendedType = other.extendedType;
                secondTag = other.secondTag;
                extended = other.extended;
                dataSize = other.dataSize;
                name = other.name;
                value = other.value;
                children.assign(other.children.begin(), other.children.end());
            }
            bool empty() const {
                return children.empty();
            }
            const TagInfo* findChild(int srcTag) const {
                auto it = std::find_if(children.begin(), children.end(), [srcTag](const TagInfo& info) {
                        return info.tag == srcTag;
                    });
                if (it == children.end()) {
                    return nullptr;
                }
                return &(*it);
            }
            const_iterator findNextChild(int srcTag, const_iterator begin) const {
                return std::find_if(begin, children.end(), [srcTag](const TagInfo& info) {
                    return info.tag == srcTag;
                    });
            }
            const vsi::TagInfo* findChild(const std::vector<int>& path) const {
                const TagInfo* current = this;
                for (const int srcTag : path) {
                    current = current->findChild(srcTag);
                    if (!current) {
                        break;
                    }
                }
                return current;
            }
        public:
            int tag = Tag::UNKNOWN;
            int fieldType = 0;
            ValueType valueType = ValueType::UNSET;
            ExtendedType extendedType = ExtendedType::UNSET;
            int secondTag = -1;
            bool extended = false;
            int32_t dataSize = 0;
            std::string name;
            TagInfos children;
            std::string value;
        };
    }
}
