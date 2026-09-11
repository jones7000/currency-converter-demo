#pragma once

#include <chrono>
#include <cstddef>
#include <string>

#include <Poco/Util/LayeredConfiguration.h>

namespace currency::config {

// Typed façade over the application's Poco::Util::LayeredConfiguration.
// Two layers feed it:
//   - a JSON file, loaded explicitly via loadJsonFile() at Application::PRIO_DEFAULT,
//   - CLI `-Dkey=value` overrides, written by main.cpp's App::handleDefine()
//     into the application's own writeable layer at Application::PRIO_APPLICATION,
//     which has a *lower* priority value and therefore wins.
// Every getter has a sensible built-in default, so a missing/partial config
// file is a warning, never a crash.
class AppConfig {
public:
    explicit AppConfig(Poco::Util::LayeredConfiguration& config) : config_(config) {}

    // Adds the given JSON file as a read-only configuration layer. Safe to
    // call only when the file exists -- callers check that first so a
    // missing file degrades to defaults instead of throwing.
    void loadJsonFile(const std::string& path);

    int grpcPort() const;

    std::string externalApiBaseUrl() const;
    std::chrono::milliseconds externalApiTimeout() const;

    std::chrono::minutes historyCacheMaxAge() const;
    std::size_t historyCacheMaxElements() const;

    std::chrono::minutes latestRateCacheMaxAge() const;
    std::size_t latestRateCacheMaxElements() const;

    // off | error | warning | information | debug.
    std::string loggingLevel() const;
    std::string loggingFile() const;

private:
    Poco::Util::LayeredConfiguration& config_;
};

} // namespace currency::config
