#include "grpc/CurrencyServiceImpl.h"

#include <chrono>
#include <fmt/format.h>

#include "domain/Constraints.h"
#include "domain/ConversionResult.h"
#include "domain/Currency.h"
#include "domain/DateRange.h"
#include "domain/Errors.h"
#include "domain/ExchangeRate.h"

namespace currency::grpcapi {

CurrencyServiceImpl::CurrencyServiceImpl(application::CurrencyCatalogService& catalog,
                                          application::ConversionService& conversion,
                                          application::HistoryService& history,
                                          application::ConstraintsService& constraints)
    : catalog_(catalog),
      conversion_(conversion),
      history_(history),
      constraints_(constraints),
      logger_(Poco::Logger::get("grpc.currency_service")) {}

grpc::Status CurrencyServiceImpl::handle(const std::string& method, const std::string& requestSummary,
                                          const std::function<void()>& work) const {
    if (logger_.information()) {
        logger_.information(fmt::format("{} request: {}", method, requestSummary));
    }

    const auto start = std::chrono::steady_clock::now();
    grpc::Status status = grpc::Status::OK;
    try {
        work();
    } catch (const std::exception& ex) {
        status = toStatus(ex);
    }
    const auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::steady_clock::now() - start)
                                 .count();

    if (logger_.information()) {
        const std::string outcome = status.ok() ? "OK" : fmt::format("{}: {}", static_cast<int>(status.error_code()),
                                                                       status.error_message());
        logger_.information(fmt::format("{} -> {} ({} ms)", method, outcome, durationMs));
    }
    return status;
}

grpc::Status CurrencyServiceImpl::toStatus(const std::exception& ex) const {
    if (const auto* e = dynamic_cast<const domain::InvalidArgumentException*>(&ex)) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, e->what());
    }
    if (const auto* e = dynamic_cast<const domain::CurrencyNotFoundException*>(&ex)) {
        return grpc::Status(grpc::StatusCode::NOT_FOUND, e->what());
    }
    if (const auto* e = dynamic_cast<const domain::ExternalApiUnavailableException*>(&ex)) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, e->what());
    }
    if (const auto* e = dynamic_cast<const domain::ExternalApiUnexpectedResponseException*>(&ex)) {
        // Detail stays server-side; the client only sees a generic message.
        if (logger_.error()) {
            logger_.error(fmt::format("unexpected upstream response: {}", e->what()));
        }
        return grpc::Status(grpc::StatusCode::INTERNAL, "internal server error");
    }
    // Catch-all: a single bad request must never crash the process.
    if (logger_.error()) {
        logger_.error(fmt::format("unhandled exception: {}", ex.what()));
    }
    return grpc::Status(grpc::StatusCode::INTERNAL, "internal server error");
}

grpc::Status CurrencyServiceImpl::ListCurrencies(grpc::ServerContext* /*context*/,
                                                  const currency::ListCurrenciesRequest* /*request*/,
                                                  currency::CurrencyListResponse* response) {
    return handle("ListCurrencies", "", [&] {
        for (const auto& currency : catalog_.listCurrencies()) {
            auto* out = response->add_currencies();
            out->set_code(currency.code);
            out->set_full_name(currency.fullName);
        }
    });
}

grpc::Status CurrencyServiceImpl::Convert(grpc::ServerContext* /*context*/,
                                           const currency::ConvertRequest* request,
                                           currency::ConvertResponse* response) {
    const std::string summary = fmt::format("{} {} -> {}", request->amount(), request->source_currency(),
                                             request->target_currency());
    return handle("Convert", summary, [&] {
        const domain::ConversionResult result =
            conversion_.convert(request->source_currency(), request->target_currency(), request->amount());
        response->set_source_amount(result.sourceAmount);
        response->set_converted_amount(result.convertedAmount);
        response->set_exchange_rate(result.exchangeRate);
        response->set_rate_date(result.rateDate);
    });
}

grpc::Status CurrencyServiceImpl::GetHistory(grpc::ServerContext* /*context*/,
                                              const currency::HistoryRequest* request,
                                              currency::HistoryResponse* response) {
    const std::string summary = fmt::format("{} -> {} [{}..{}]", request->source_currency(),
                                             request->target_currency(), request->start_date(), request->end_date());
    return handle("GetHistory", summary, [&] {
        const domain::DateRange range(request->start_date(), request->end_date());
        const auto points = history_.getHistory(request->source_currency(), request->target_currency(), range);

        response->set_source_currency(request->source_currency());
        response->set_target_currency(request->target_currency());
        for (const auto& point : points) {
            auto* out = response->add_points();
            out->set_date(point.date);
            out->set_rate(point.rate);
        }
    });
}

grpc::Status CurrencyServiceImpl::GetConstraints(grpc::ServerContext* /*context*/,
                                                  const currency::ConstraintsRequest* /*request*/,
                                                  currency::ConstraintsResponse* response) {
    return handle("GetConstraints", "", [&] {
        const domain::Constraints constraints = constraints_.getConstraints();
        response->set_min_amount(constraints.minAmount);
        response->set_max_amount(constraints.maxAmount);
        response->set_min_date(constraints.minDate);
        response->set_max_date(constraints.maxDate);
    });
}

} // namespace currency::grpcapi
