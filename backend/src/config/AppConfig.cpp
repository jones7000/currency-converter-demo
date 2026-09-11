#include "config/AppConfig.h"

#include <Poco/AutoPtr.h>
#include <Poco/Util/Application.h>
#include <Poco/Util/JSONConfiguration.h>

namespace currency::config {

void AppConfig::loadJsonFile(const std::string& path) {
    Poco::AutoPtr<Poco::Util::JSONConfiguration> jsonConfig(new Poco::Util::JSONConfiguration(path));
    config_.add(jsonConfig, Poco::Util::Application::PRIO_DEFAULT);
}

int AppConfig::grpcPort() const {
    return config_.getInt("server.grpc_port", 9090);
}

std::string AppConfig::externalApiBaseUrl() const {
    return config_.getString("external_api.base_url", "https://api.frankfurter.dev");
}

std::chrono::milliseconds AppConfig::externalApiTimeout() const {
    return std::chrono::milliseconds(config_.getInt("external_api.timeout_ms", 5000));
}

std::chrono::minutes AppConfig::historyCacheMaxAge() const {
    return std::chrono::minutes(config_.getInt("cache.history.max_age_minutes", 1440));
}

std::size_t AppConfig::historyCacheMaxElements() const {
    return static_cast<std::size_t>(config_.getInt("cache.history.max_elements", 5000));
}

std::chrono::minutes AppConfig::latestRateCacheMaxAge() const {
    return std::chrono::minutes(config_.getInt("cache.latest_rate.max_age_minutes", 15));
}

std::size_t AppConfig::latestRateCacheMaxElements() const {
    return static_cast<std::size_t>(config_.getInt("cache.latest_rate.max_elements", 500));
}

std::string AppConfig::loggingLevel() const {
    return config_.getString("logging.level", "information");
}

std::string AppConfig::loggingFile() const {
    return config_.getString("logging.file", "/var/log/currency-server/backend.log");
}

} // namespace currency::config
