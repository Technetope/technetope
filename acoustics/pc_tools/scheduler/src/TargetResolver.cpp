#include "acoustics/scheduler/TargetResolver.h"

#include <algorithm>
#include <unordered_set>

namespace acoustics::scheduler {

namespace {

void appendUniquePreserveOrder(std::vector<std::string>& dest,
                               const std::vector<std::string>& values) {
    std::unordered_set<std::string> seen(dest.begin(), dest.end());
    for (const auto& value : values) {
        if (seen.insert(value).second) {
            dest.push_back(value);
        }
    }
}

std::vector<std::string> deduplicate(std::vector<std::string> values) {
    std::unordered_set<std::string> seen;
    std::vector<std::string> unique;
    unique.reserve(values.size());
    for (auto& value : values) {
        if (seen.insert(value).second) {
            unique.push_back(std::move(value));
        }
    }
    return unique;
}

}  // namespace

void TargetResolver::setMapping(std::unordered_map<std::string, std::vector<std::string>> mapping) {
    for (auto& [key, list] : mapping) {
        list = deduplicate(std::move(list));
    }
    mapping_ = std::move(mapping);
    rebuildKnownDevices();
}

void TargetResolver::setDefaultTargets(std::vector<std::string> defaults) {
    defaultTargets_ = deduplicate(std::move(defaults));
}

std::vector<std::string> TargetResolver::resolve(const std::vector<std::string>& requested) const {
    std::vector<std::string> resolved;
    if (requested.empty()) {
        if (!defaultTargets_.empty()) {
            resolved = defaultTargets_;
        } else {
            resolved = knownDevices_;
        }
        return resolved;
    }

    resolved.reserve(requested.size());
    std::unordered_set<std::string> seen;
    for (const auto& target : requested) {
        auto it = mapping_.find(target);
        if (it != mapping_.end()) {
            for (const auto& deviceId : it->second) {
                if (seen.insert(deviceId).second) {
                    resolved.push_back(deviceId);
                }
            }
        } else {
            if (seen.insert(target).second) {
                resolved.push_back(target);
            }
        }
    }
    return resolved;
}

std::vector<std::string> TargetResolver::resolveDefault() const {
    return resolve({});
}

void TargetResolver::rebuildKnownDevices() {
    knownDevices_.clear();
    for (const auto& [_, devices] : mapping_) {
        appendUniquePreserveOrder(knownDevices_, devices);
    }
}

}  // namespace acoustics::scheduler
