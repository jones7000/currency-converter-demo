#include "application/ConversionService.h"

#include <cmath>
#include <fmt/format.h>

#include "domain/Errors.h"
#include "domain/TextUtil.h"

namespace currency::application {

ConversionService::ConversionService(external::IExchangeRateProvider& provider,
                                      cache::ICache<domain::ExchangeRate>& latestRateCache,
                                      CurrencyCatalogService& catalog,
                                      ConstraintsService& constraints)
    : provider_(provider),
      latestRateCache_(latestRateCache),
      catalog_(catalog),
      constraints_(constraints),
      logger_(Poco::Logger::get("application.conversion")) {}

domain::ConversionResult ConversionService::convert(const std::string& sourceCurrency,
                                                      const std::string& targetCurrency,
                                                      double amount) {
    const std::string source = domain::toUpper(sourceCurrency);
    const std::string target = domain::toUpper(targetCurrency);

    // 1. Currency codes must be known.
    if (!catalog_.isKnownCurrency(source)) {
        throw domain::CurrencyNotFoundException(fmt::format("unknown source currency '{}'", source));
    }
    if (!catalog_.isKnownCurrency(target)) {
        throw domain::CurrencyNotFoundException(fmt::format("unknown target currency '{}'", target));
    }

    // 2. A no-op conversion request most likely indicates a client-side
    // selection bug and should surface, not be silently masked with rate 1.0.
    if (source == target) {
        throw domain::InvalidArgumentException("source_currency and target_currency must differ");
    }

    // 3. Amount must be finite and within the server-declared bounds.
    const domain::Constraints constraints = constraints_.getConstraints();
    if (!std::isfinite(amount) || amount < constraints.minAmount || amount > constraints.maxAmount) {
        throw domain::InvalidArgumentException(
            fmt::format("amount must be between {} and {}", constraints.minAmount, constraints.maxAmount));
    }

    const std::string cacheKey = fmt::format("{}|{}", source, target);
    domain::ExchangeRate rate;
    if (auto cached = latestRateCache_.get(cacheKey)) {
        if (logger_.information()) {
            logger_.information(fmt::format("cache hit for latest rate {}", cacheKey));
        }
        rate = *cached;
    } else {
        if (logger_.information()) {
            logger_.information(fmt::format("cache miss for latest rate {}, fetching from frankfurter.dev", cacheKey));
        }
        rate = provider_.latestRate(source, target);
        latestRateCache_.put(cacheKey, rate);
    }

    return domain::ConversionResult{
        .sourceAmount = amount,
        .convertedAmount = amount * rate.rate,
        .exchangeRate = rate.rate,
        .rateDate = rate.date,
    };
}

} // namespace currency::application
