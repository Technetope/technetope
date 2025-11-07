#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace acoustics::scheduler {

/**
 * @brief Resolves logical targets defined in a timeline into concrete device IDs.
 *
 * Targets in a timeline may already be device identifiers or can reference logical
 * group names (e.g., voices).  The resolver allows the caller to provide a mapping
 * for these logical names and also define a default target list that represents
 * the "all devices" group.
 */
class TargetResolver {
public:
    TargetResolver() = default;

    /**
     * @brief Populate mapping entries (logical name -> list of device IDs).
     *
     * The resolver stores a copy of the provided map.  Any duplicate device IDs
     * within a single mapping entry are collapsed.
     */
    void setMapping(std::unordered_map<std::string, std::vector<std::string>> mapping);

    /**
     * @brief Set the default device list used when an event omits the targets field.
     */
    void setDefaultTargets(std::vector<std::string> defaults);

    /**
     * @brief Resolve the requested targets into concrete device IDs.
     *
     * - When @p requested is empty, the resolver returns the configured default
     *   target list. If no explicit default is configured, the union of all known
     *   device IDs from the mapping is returned. If the resolver has no knowledge
     *   of devices, an empty list is produced.
     * - When @p requested contains values, each value is matched against the
     *   mapping. Missing entries are treated as literal device IDs and therefore
     *   passed through unchanged.
     * - The result never contains duplicates and preserves the first-seen order.
     */
    std::vector<std::string> resolve(const std::vector<std::string>& requested) const;

    /**
     * @brief Convenience wrapper equivalent to resolve({}).
     */
    std::vector<std::string> resolveDefault() const;

private:
    void rebuildKnownDevices();

    std::unordered_map<std::string, std::vector<std::string>> mapping_;
    std::vector<std::string> defaultTargets_;
    std::vector<std::string> knownDevices_;
};

}  // namespace acoustics::scheduler
