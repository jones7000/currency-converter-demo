#include "external/FrankfurterClient.h"

#include <algorithm>
#include <fmt/format.h>
#include <memory>
#include <sstream>

#include <Poco/Exception.h>
#include <Poco/JSON/Array.h>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Parser.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/StreamCopier.h>
#include <Poco/Timespan.h>
#include <Poco/URI.h>

#include "domain/Errors.h"

namespace currency::external {

using currency::domain::Currency;
using currency::domain::DateRange;
using currency::domain::ExchangeRate;
using currency::domain::ExternalApiUnavailableException;
using currency::domain::ExternalApiUnexpectedResponseException;

FrankfurterClient::FrankfurterClient(std::string baseUrl, std::chrono::milliseconds timeout)
    : baseUrl_(std::move(baseUrl)),
      timeout_(timeout),
      logger_(Poco::Logger::get("external.frankfurter")) {}

std::string FrankfurterClient::get(const std::string& path) const {
    Poco::URI uri(baseUrl_ + path);

    std::unique_ptr<Poco::Net::HTTPClientSession> session;
    if (uri.getScheme() == "https") {
        session = std::make_unique<Poco::Net::HTTPSClientSession>(uri.getHost(), uri.getPort());
    } else {
        session = std::make_unique<Poco::Net::HTTPClientSession>(uri.getHost(), uri.getPort());
    }
    session->setTimeout(Poco::Timespan(
        static_cast<long>(timeout_.count() / 1000),
        static_cast<long>((timeout_.count() % 1000) * 1000)));

    std::string pathAndQuery = uri.getPathAndQuery();
    if (pathAndQuery.empty()) {
        pathAndQuery = "/";
    }

    Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, pathAndQuery,
                                    Poco::Net::HTTPMessage::HTTP_1_1);
    request.set("User-Agent", "currency-converter-backend/1.0");

    const auto start = std::chrono::steady_clock::now();
    try {
        session->sendRequest(request);

        Poco::Net::HTTPResponse response;
        std::istream& responseStream = session->receiveResponse(response);

        std::ostringstream body;
        Poco::StreamCopier::copyStream(responseStream, body);

        const auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                     std::chrono::steady_clock::now() - start)
                                     .count();

        if (response.getStatus() != Poco::Net::HTTPResponse::HTTP_OK) {
            if (logger_.warning()) {
                logger_.warning(fmt::format("GET {} -> HTTP {} ({} ms)", pathAndQuery,
                                             static_cast<int>(response.getStatus()), durationMs));
            }
            throw ExternalApiUnavailableException(
                fmt::format("frankfurter.dev returned HTTP {}", static_cast<int>(response.getStatus())));
        }

        if (logger_.information()) {
            logger_.information(fmt::format("GET {} -> HTTP {} ({} ms)", pathAndQuery,
                                             static_cast<int>(response.getStatus()), durationMs));
        }
        return body.str();
    } catch (const ExternalApiUnavailableException&) {
        throw;
    } catch (const Poco::Exception& ex) {
        const auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                     std::chrono::steady_clock::now() - start)
                                     .count();
        if (logger_.warning()) {
            logger_.warning(
                fmt::format("GET {} failed after {} ms: {}", pathAndQuery, durationMs, ex.displayText()));
        }
        throw ExternalApiUnavailableException(
            fmt::format("could not reach frankfurter.dev: {}", ex.displayText()));
    }
}

std::vector<Currency> FrankfurterClient::listCurrencies() {
    const std::string body = get("/v1/currencies");
    try {
        Poco::JSON::Parser parser;
        auto object = parser.parse(body).extract<Poco::JSON::Object::Ptr>();

        std::vector<Currency> currencies;
        currencies.reserve(object->size());
        for (const auto& entry : *object) {
            currencies.push_back(Currency{entry.first, entry.second.toString()});
        }
        return currencies;
    } catch (const Poco::Exception& ex) {
        throw ExternalApiUnexpectedResponseException(
            fmt::format("unexpected /v1/currencies payload: {}", ex.displayText()));
    }
}

ExchangeRate FrankfurterClient::latestRate(const std::string& sourceCurrency,
                                            const std::string& targetCurrency) {
    const std::string path = fmt::format("/v1/latest?base={}&symbols={}", sourceCurrency, targetCurrency);
    const std::string body = get(path);
    try {
        Poco::JSON::Parser parser;
        auto object = parser.parse(body).extract<Poco::JSON::Object::Ptr>();

        const std::string date = object->getValue<std::string>("date");
        Poco::JSON::Object::Ptr rates = object->getObject("rates");
        if (!rates || !rates->has(targetCurrency)) {
            throw ExternalApiUnexpectedResponseException(
                fmt::format("no rate for {} in /v1/latest response", targetCurrency));
        }
        const double rate = rates->getValue<double>(targetCurrency);
        return ExchangeRate{sourceCurrency, targetCurrency, rate, date};
    } catch (const ExternalApiUnexpectedResponseException&) {
        throw;
    } catch (const Poco::Exception& ex) {
        throw ExternalApiUnexpectedResponseException(
            fmt::format("unexpected /v1/latest payload: {}", ex.displayText()));
    }
}

std::vector<ExchangeRate> FrankfurterClient::history(const std::string& sourceCurrency,
                                                       const std::string& targetCurrency,
                                                       const DateRange& range) {
    const std::string path = fmt::format("/v1/{}..{}?base={}&symbols={}", range.startDate(),
                                          range.endDate(), sourceCurrency, targetCurrency);
    const std::string body = get(path);
    try {
        Poco::JSON::Parser parser;
        auto object = parser.parse(body).extract<Poco::JSON::Object::Ptr>();

        Poco::JSON::Object::Ptr ratesByDate = object->getObject("rates");
        if (!ratesByDate) {
            throw ExternalApiUnexpectedResponseException("missing 'rates' in /v1/{start}..{end} response");
        }

        std::vector<ExchangeRate> points;
        points.reserve(ratesByDate->size());
        for (const auto& entry : *ratesByDate) {
            const std::string& date = entry.first;
            // frankfurter.dev sometimes includes one point *before*
            // range.startDate() when the start date itself isn't a trading
            // day (it substitutes the closest earlier rate). GetHistory's
            // range is closed-inclusive, so that point is filtered out here
            // rather than leaking upstream behavior into the response.
            if (date < range.startDate() || date > range.endDate()) {
                continue;
            }
            Poco::JSON::Object::Ptr dayRates = entry.second.extract<Poco::JSON::Object::Ptr>();
            if (!dayRates || !dayRates->has(targetCurrency)) {
                continue;
            }
            points.push_back(
                ExchangeRate{sourceCurrency, targetCurrency, dayRates->getValue<double>(targetCurrency), date});
        }
        std::sort(points.begin(), points.end(),
                  [](const ExchangeRate& a, const ExchangeRate& b) { return a.date < b.date; });
        return points;
    } catch (const ExternalApiUnexpectedResponseException&) {
        throw;
    } catch (const Poco::Exception& ex) {
        throw ExternalApiUnexpectedResponseException(
            fmt::format("unexpected /v1/{{start}}..{{end}} payload: {}", ex.displayText()));
    }
}

} // namespace currency::external
