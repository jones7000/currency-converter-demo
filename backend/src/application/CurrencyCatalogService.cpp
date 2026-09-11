#include "application/CurrencyCatalogService.h"

#include <algorithm>

#include "domain/Errors.h"
#include "domain/TextUtil.h"

namespace currency::application {

CurrencyCatalogService::CurrencyCatalogService(external::IExchangeRateProvider& provider,
                                                 cache::ICache<std::vector<domain::Currency>>& cache)
    : provider_(provider), cache_(cache), logger_(Poco::Logger::get("application.catalog")) {}

std::vector<domain::Currency> CurrencyCatalogService::listCurrencies() {
    if (auto cached = cache_.get(kCacheKey)) {
        if (logger_.information()) {
            logger_.information("cache hit for currency list");
        }
        return *cached;
    }

    if (logger_.information()) {
        logger_.information("cache miss for currency list, fetching from frankfurter.dev");
    }
    auto currencies = provider_.listCurrencies();
    if (currencies.empty()) {
        throw domain::ExternalApiUnexpectedResponseException("frankfurter.dev returned an empty currency list");
    }
    cache_.put(kCacheKey, currencies);
    return currencies;
}

bool CurrencyCatalogService::isKnownCurrency(const std::string& code) {
    const std::string normalized = domain::toUpper(code);
    auto currencies = listCurrencies();
    return std::any_of(currencies.begin(), currencies.end(),
                        [&normalized](const domain::Currency& c) { return c.code == normalized; });
}

} // namespace currency::application
