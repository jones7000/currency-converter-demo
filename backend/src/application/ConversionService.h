#pragma once

#include <string>

#include <Poco/Logger.h>

#include "application/ConstraintsService.h"
#include "application/CurrencyCatalogService.h"
#include "cache/ICache.h"
#include "domain/ConversionResult.h"
#include "domain/ExchangeRate.h"
#include "external/IExchangeRateProvider.h"

namespace currency::application {

// Convert: validates the request, resolves the latest rate (cache-or-fetch),
// and computes the converted amount. All business rules for conversion live
// here, not in the gRPC layer.
class ConversionService {
public:
    ConversionService(external::IExchangeRateProvider& provider,
                       cache::ICache<domain::ExchangeRate>& latestRateCache,
                       CurrencyCatalogService& catalog,
                       ConstraintsService& constraints);

    domain::ConversionResult convert(const std::string& sourceCurrency,
                                      const std::string& targetCurrency,
                                      double amount);

private:
    external::IExchangeRateProvider& provider_;
    cache::ICache<domain::ExchangeRate>& latestRateCache_;
    CurrencyCatalogService& catalog_;
    ConstraintsService& constraints_;
    Poco::Logger& logger_;
};

} // namespace currency::application
