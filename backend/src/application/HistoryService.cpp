#include "application/HistoryService.h"

#include <fmt/format.h>

#include "domain/Errors.h"
#include "domain/TextUtil.h"

namespace currency::application {

HistoryService::HistoryService(external::IExchangeRateProvider& provider,
                                cache::ICache<std::vector<domain::ExchangeRate>>& historyCache,
                                CurrencyCatalogService& catalog,
                                ConstraintsService& constraints)
    : provider_(provider),
      historyCache_(historyCache),
      catalog_(catalog),
      constraints_(constraints),
      logger_(Poco::Logger::get("application.history")) {}

std::vector<domain::ExchangeRate> HistoryService::getHistory(const std::string& sourceCurrency,
                                                               const std::string& targetCurrency,
                                                               const domain::DateRange& range) {
    const std::string source = domain::toUpper(sourceCurrency);
    const std::string target = domain::toUpper(targetCurrency);

    // 1. Currency codes must be known.
    if (!catalog_.isKnownCurrency(source)) {
        throw domain::CurrencyNotFoundException(fmt::format("unknown source currency '{}'", source));
    }
    if (!catalog_.isKnownCurrency(target)) {
        throw domain::CurrencyNotFoundException(fmt::format("unknown target currency '{}'", target));
    }

    // 2. start <= end was already enforced by domain::DateRange's
    // constructor. 3. Both dates must fall within the server-declared
    // selectable range.
    const domain::Constraints constraints = constraints_.getConstraints();
    if (range.startDate() < constraints.minDate || range.endDate() > constraints.maxDate) {
        throw domain::InvalidArgumentException(
            fmt::format("date range must fall within [{}, {}]", constraints.minDate, constraints.maxDate));
    }

    // Cache key is the exact query tuple: a repeated query for data that is
    // already cached is served from cache -- no partial-range reuse.
    const std::string cacheKey = fmt::format("{}|{}|{}|{}", source, target, range.startDate(), range.endDate());
    if (auto cached = historyCache_.get(cacheKey)) {
        if (logger_.information()) {
            logger_.information(fmt::format("cache hit for history {}", cacheKey));
        }
        return *cached;
    }

    if (logger_.information()) {
        logger_.information(fmt::format("cache miss for history {}, fetching from frankfurter.dev", cacheKey));
    }
    auto points = provider_.history(source, target, range);
    historyCache_.put(cacheKey, points);
    return points;
}

} // namespace currency::application
