#pragma once

#include <string>
#include <vector>

#include <Poco/Logger.h>

#include "application/ConstraintsService.h"
#include "application/CurrencyCatalogService.h"
#include "cache/ICache.h"
#include "domain/DateRange.h"
#include "domain/ExchangeRate.h"
#include "external/IExchangeRateProvider.h"

namespace currency::application {

// Historical trend lookup: validates the request and resolves the rate
// series for a date range (cache-or-fetch). This is the endpoint that
// mandatory caching applies to.
class HistoryService {
public:
    HistoryService(external::IExchangeRateProvider& provider,
                    cache::ICache<std::vector<domain::ExchangeRate>>& historyCache,
                    CurrencyCatalogService& catalog,
                    ConstraintsService& constraints);

    std::vector<domain::ExchangeRate> getHistory(const std::string& sourceCurrency,
                                                  const std::string& targetCurrency,
                                                  const domain::DateRange& range);

private:
    external::IExchangeRateProvider& provider_;
    cache::ICache<std::vector<domain::ExchangeRate>>& historyCache_;
    CurrencyCatalogService& catalog_;
    ConstraintsService& constraints_;
    Poco::Logger& logger_;
};

} // namespace currency::application
