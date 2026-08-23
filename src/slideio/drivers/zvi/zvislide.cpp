// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/zvi/zvislide.hpp"
#include "slideio/drivers/zvi/zviscene.hpp"
#include "slideio/drivers/zvi/zviimageitem.hpp"
#include "slideio/drivers/zvi/zviutils.hpp"
#include "slideio/drivers/zvi/zvitags.hpp"
#include "slideio/core/metadata_internal.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/core/log.hpp"
#include <nlohmann/json.hpp>
#include <pole/storage.hpp>
#include <map>
#include <optional>
#include <type_traits>
#include <variant>

using namespace slideio;

ZVISlide::ZVISlide(const std::string& filePath, const std::string& driverId) : m_filePath(filePath)
{
	setDriverId(driverId);
	init();
}

int ZVISlide::getNumScenes() const { return 1; }
std::string ZVISlide::getFilePath() const { return ""; }

std::shared_ptr<CVScene> ZVISlide::getScene(int index) const
{
	if (index != 0) {
		throw std::runtime_error("ZVIImageDriver: Invalid scene index");
	}
	return m_scene;
}

double ZVISlide::getMagnification() const { return 0; }
Resolution ZVISlide::getResolution() const { return Resolution(); }
double ZVISlide::getZSliceResolution() const { return 0; }
double ZVISlide::getTFrameResolution() const { return 0; }

namespace {

nlohmann::json variantToJson(const ZVIUtils::Variant& v)
{
    return std::visit([](const auto& x) -> nlohmann::json {
        using T = std::decay_t<decltype(x)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            return nullptr;
        } else if constexpr (std::is_same_v<T, uint32_t> || std::is_same_v<T, uint64_t>) {
            return static_cast<int64_t>(x);
        } else {
            return x;
        }
    }, v);
}

// Convert a tag-entry vector into a JSON object. Repeated tag ids are
// collapsed into a JSON array in encounter order: first occurrence is
// scalar, second turns it into an array of [first, second], subsequent
// occurrences append.
nlohmann::json tagsToJsonObject(const std::vector<ZVIUtils::ZviTagEntry>& entries)
{
    nlohmann::json obj = nlohmann::json::object();
    for (const auto& e : entries) {
        const char* name = getZviTagName(e.id);
        const std::string key = name ? std::string(name)
                                     : "Tag_" + std::to_string(e.id);
        nlohmann::json value = variantToJson(e.value);

        auto it = obj.find(key);
        if (it == obj.end()) {
            obj[key] = std::move(value);
        } else if (it->is_array()) {
            it->push_back(std::move(value));
        } else {
            nlohmann::json arr = nlohmann::json::array();
            arr.push_back(std::move(*it));
            arr.push_back(std::move(value));
            *it = std::move(arr);
        }
    }
    return obj;
}

// For every key present in all `children` with identical values, remove
// the key from each child and place it on `parent`. Mutates both.
void hoistInvariantTags(nlohmann::json& parent,
                        std::vector<nlohmann::json*>& children)
{
    if (children.empty()) return;

    // Collect candidate keys from the first child.
    nlohmann::json& first = *children.front();
    std::vector<std::string> candidates;
    candidates.reserve(first.size());
    for (auto it = first.begin(); it != first.end(); ++it) {
        candidates.push_back(it.key());
    }

    for (const std::string& key : candidates) {
        const nlohmann::json& firstVal = first.at(key);
        bool invariant = true;
        for (size_t i = 1; i < children.size(); ++i) {
            auto it = children[i]->find(key);
            if (it == children[i]->end() || *it != firstVal) {
                invariant = false;
                break;
            }
        }
        if (invariant) {
            parent[key] = firstVal;
            for (auto* child : children) {
                child->erase(key);
            }
        }
    }
}

// Tries to read a Tags stream from the open OLE document at `path`.
// Returns std::nullopt on any failure (missing stream, parse error).
std::optional<nlohmann::json> tryReadTagsObject(ole::compound_document& doc,
                                                const std::string& path,
                                                bool hasClsidHeader)
{
    try {
        ZVIUtils::StreamKeeper s(doc, path);
        return tagsToJsonObject(ZVIUtils::readAllTags(s, hasClsidHeader));
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

} // namespace

void ZVISlide::init()
{
	m_scene.reset(new ZVIScene(m_filePath, getDriverId()));
	auto scene = std::dynamic_pointer_cast<ZVIScene>(m_scene);
	if (!scene) {
		return;
	}

#if defined(WIN32)
	ole::compound_document doc(Tools::toWstring(m_filePath));
#else
	ole::compound_document doc(m_filePath);
#endif
	if (!doc.good()) {
		return;
	}

	nlohmann::json root = nlohmann::json::object();

	// File-level /Image/Tags/Contents.
	try {
		ZVIUtils::StreamKeeper s(doc, "/Image/Tags/Contents");
		root = tagsToJsonObject(ZVIUtils::readAllTags(s, false));
	} catch (const std::exception& e) {
		SLIDEIO_LOG(WARNING) << "ZVIImageDriver: failed to read /Image/Tags/Contents: " << e.what();
	}

	// Optional root-level <Tags>. Silent on failure (normal for older files).
	if (auto rootTags = tryReadTagsObject(doc, "/Tags", true)) {
		for (auto it = rootTags->begin(); it != rootTags->end(); ++it) {
			root[it.key()] = it.value();
		}
	}

	// Per-item streams, bucketed by (C, Z, T).
	const int numChannels = scene->getNumChannels();
	const int numZSlices  = scene->getNumZSlices();
	const int numTFrames  = scene->getNumTFrames();

	// items[c][z][t] = parsed per-item tag object.
	std::map<int, std::map<int, std::map<int, nlohmann::json>>> items;

	for (const ZVIImageItem& item : scene->getImageItems()) {
		const int c = item.getCIndex();
		const int z = item.getZIndex();
		const int t = item.getTIndex();
		const std::string path =
			"/Image/Item(" + std::to_string(item.getItemIndex()) + ")/Tags/Contents";
		auto obj = tryReadTagsObject(doc, path, false);
		if (obj) {
			items[c][z][t] = std::move(*obj);
		} else {
			items[c][z][t] = nlohmann::json::object();
		}
	}

	if (!items.empty()) {
		nlohmann::json channelsArr = nlohmann::json::array();

		for (int c = 0; c < numChannels; ++c) {
			nlohmann::json channelObj = nlohmann::json::object();
			auto channelIt = items.find(c);

			if (channelIt == items.end()) {
				channelsArr.push_back(channelObj);
				continue;
			}

			if (numZSlices > 1) {
				nlohmann::json zArr = nlohmann::json::array();
				for (int z = 0; z < numZSlices; ++z) {
					nlohmann::json zObj = nlohmann::json::object();
					auto zIt = channelIt->second.find(z);

					if (zIt != channelIt->second.end()) {
						if (numTFrames > 1) {
							nlohmann::json tArr = nlohmann::json::array();
							std::vector<nlohmann::json*> tPtrs;
							for (int t = 0; t < numTFrames; ++t) {
								auto tIt = zIt->second.find(t);
								tArr.push_back(tIt != zIt->second.end()
								                ? std::move(tIt->second)
								                : nlohmann::json::object());
							}
							for (auto& tElem : tArr) tPtrs.push_back(&tElem);
							hoistInvariantTags(zObj, tPtrs);
							zObj["TFrames"] = std::move(tArr);
						} else {
							// Single t-frame: lift its tags directly onto zObj.
							auto tIt = zIt->second.find(0);
							if (tIt != zIt->second.end()) {
								zObj = std::move(tIt->second);
							}
						}
					}
					zArr.push_back(std::move(zObj));
				}
				std::vector<nlohmann::json*> zPtrs;
				for (auto& zElem : zArr) zPtrs.push_back(&zElem);
				hoistInvariantTags(channelObj, zPtrs);
				channelObj["ZSlices"] = std::move(zArr);
			} else {
				// Single z-slice.
				auto zIt = channelIt->second.find(0);
				if (zIt != channelIt->second.end()) {
					if (numTFrames > 1) {
						nlohmann::json tArr = nlohmann::json::array();
						std::vector<nlohmann::json*> tPtrs;
						for (int t = 0; t < numTFrames; ++t) {
							auto tIt = zIt->second.find(t);
							tArr.push_back(tIt != zIt->second.end()
							                ? std::move(tIt->second)
							                : nlohmann::json::object());
						}
						for (auto& tElem : tArr) tPtrs.push_back(&tElem);
						hoistInvariantTags(channelObj, tPtrs);
						channelObj["TFrames"] = std::move(tArr);
					} else {
						auto tIt = zIt->second.find(0);
						if (tIt != zIt->second.end()) {
							channelObj = std::move(tIt->second);
						}
					}
				}
			}

			channelsArr.push_back(std::move(channelObj));
		}

		root["Channels"] = std::move(channelsArr);
	}

	if (!root.empty()) {
		m_rawMetadata = root.dump(2);
		m_metadataFormat = MetadataFormat::JSON;
	}
}

MetadataBuilder ZVISlide::buildMetadataTree() const
{
	if (m_rawMetadata.empty()) {
		return CVSlide::buildMetadataTree();
	}
	return detail::builderFromJson(nlohmann::json::parse(m_rawMetadata));
}
