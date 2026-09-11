#pragma once

#include <vector>

#include <Poco/Logger.h>

#include "cache/ICache.h"
#include "domain/Currency.h"
#include "external/IExchangeRateProvider.h"

namespace currency::application {

// Currency overview listing and the shared currency-code validation used by
// ConversionService/HistoryService. The currency list changes essentially
// never, so it is cached like everything else this backend fetches from
// frankfurter.dev, even though mandatory caching is only strictly required
// for historical data.
class CurrencyCatalogService {
public:
    CurrencyCatalogService(external::IExchangeRateProvider& provider,
                            cache::ICache<std::vector<domain::Currency>>& cache);

    std::vector<domain::Currency> listCurrencies();

    bool isKnownCurrency(const std::string& code);

private:
    static constexpr const char* kCacheKey = "currencies";

    external::IExchangeRateProvider& provider_;
    cache::ICache<std::vector<domain::Currency>>& cache_;
    Poco::Logger& logger_;
};

} // namespace currency::application
