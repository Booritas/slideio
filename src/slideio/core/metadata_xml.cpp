// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/core/metadata_internal.hpp"
#include <nlohmann/json.hpp>
#include <tinyxml2.h>
#include <string>

namespace slideio
{
    namespace detail
    {

        using nlohmann::json;
        using tinyxml2::XMLElement;

        namespace
        {
            json elementToJson(const XMLElement* el);

            void addChild(json& parent, const std::string& key, json child)
            {
                auto it = parent.find(key);
                if (it == parent.end())
                {
                    parent[key] = std::move(child);
                    return;
                }
                if (it->is_array())
                {
                    it->push_back(std::move(child));
                    return;
                }
                json arr = json::array();
                arr.push_back(std::move(*it));
                arr.push_back(std::move(child));
                *it = std::move(arr);
            }

            json elementToJson(const XMLElement* el)
            {
                json node = json::object();

                for (const auto* a = el->FirstAttribute(); a; a = a->Next())
                    node[std::string("@") + a->Name()] = a->Value();

                bool hasElementChild = false;
                for (const XMLElement* c = el->FirstChildElement(); c; c = c->NextSiblingElement())
                {
                    hasElementChild = true;
                    addChild(node, c->Name(), elementToJson(c));
                }

                const char* txt = el->GetText();
                if (txt && *txt)
                {
                    if (!hasElementChild && node.empty()) return json(txt);
                    node["#text"] = txt;
                }
                return node;
            }
        } // namespace

        json xmlStringToJson(const std::string& xml)
        {
            tinyxml2::XMLDocument doc;
            if (doc.Parse(xml.c_str(), xml.size()) != tinyxml2::XML_SUCCESS)
            {
                const char* err = doc.ErrorStr();
                return json{{"#error", err ? err : "xml parse error"}};
            }
            const XMLElement* root = doc.RootElement();
            if (!root) return json::object();
            json out = json::object();
            out[root->Name()] = elementToJson(root);
            return out;
        }

    } // namespace detail
} // namespace slideio
