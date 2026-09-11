#pragma once

#include <functional>
#include <string>

#include <Poco/Logger.h>
#include <grpcpp/grpcpp.h>

#include "application/ConstraintsService.h"
#include "application/ConversionService.h"
#include "application/CurrencyCatalogService.h"
#include "application/HistoryService.h"
#include "currency.grpc.pb.h"

namespace currency::grpcapi {

// Transport layer: translates between currency.CurrencyService's proto
// messages and the application services' domain types, and maps thrown
// domain::DomainException subtypes to grpc::StatusCode. Deliberately thin --
// no business logic lives here. Also logs one INFORMATION line per RPC
// (entry + resulting status/duration).
class CurrencyServiceImpl final : public currency::CurrencyService::Service {
public:
    CurrencyServiceImpl(application::CurrencyCatalogService& catalog,
                         application::ConversionService& conversion,
                         application::HistoryService& history,
                         application::ConstraintsService& constraints);

    grpc::Status ListCurrencies(grpc::ServerContext* context, const currency::ListCurrenciesRequest* request,
                                 currency::CurrencyListResponse* response) override;

    grpc::Status Convert(grpc::ServerContext* context, const currency::ConvertRequest* request,
                          currency::ConvertResponse* response) override;

    grpc::Status GetHistory(grpc::ServerContext* context, const currency::HistoryRequest* request,
                             currency::HistoryResponse* response) override;

    grpc::Status GetConstraints(grpc::ServerContext* context, const currency::ConstraintsRequest* request,
                                 currency::ConstraintsResponse* response) override;

private:
    // Logs entry ("method: summary") and exit (status + duration), runs
    // `work` inside a try/catch, and maps any thrown exception to a
    // grpc::Status via toStatus(). One place, used by all four RPCs, so the
    // logging/error-mapping shape can't drift between methods.
    grpc::Status handle(const std::string& method, const std::string& requestSummary,
                         const std::function<void()>& work) const;

    grpc::Status toStatus(const std::exception& ex) const;

    application::CurrencyCatalogService& catalog_;
    application::ConversionService& conversion_;
    application::HistoryService& history_;
    application::ConstraintsService& constraints_;
    Poco::Logger& logger_;
};

} // namespace currency::grpcapi
