#include <chrono>
#include <fmt/format.h>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

#include <Poco/AutoPtr.h>
#include <Poco/ConsoleChannel.h>
#include <Poco/File.h>
#include <Poco/FileChannel.h>
#include <Poco/FormattingChannel.h>
#include <Poco/Logger.h>
#include <Poco/Message.h>
#include <Poco/Net/NetSSL.h>
#include <Poco/PatternFormatter.h>
#include <Poco/SplitterChannel.h>
#include <Poco/Util/HelpFormatter.h>
#include <Poco/Util/Option.h>
#include <Poco/Util/OptionCallback.h>
#include <Poco/Util/OptionException.h>
#include <Poco/Util/OptionSet.h>
#include <Poco/Util/ServerApplication.h>

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>

#include "application/ConstraintsService.h"
#include "application/ConversionService.h"
#include "application/CurrencyCatalogService.h"
#include "application/HistoryService.h"
#include "cache/TimeAndSizeBoundedCache.h"
#include "config/AppConfig.h"
#include "domain/Currency.h"
#include "domain/ExchangeRate.h"
#include "external/FrankfurterClient.h"
#include "grpc/CurrencyServiceImpl.h"

namespace {

// Our config vocabulary is "off | error | warning | information | debug";
// Poco::Logger::setLevel(const std::string&) accepts the same names except
// "off", which is Poco's "none".
std::string toPocoLevel(const std::string& level) {
    return level == "off" ? "none" : level;
}

} // namespace

// Composition root: wires the concrete FrankfurterClient and
// TimeAndSizeBoundedCache instances into the application services, then
// into the thin gRPC transport layer. No other file in this backend
// constructs these concrete infrastructure types.
class CurrencyServerApp : public Poco::Util::ServerApplication {
protected:
    void defineOptions(Poco::Util::OptionSet& options) override {
        Poco::Util::ServerApplication::defineOptions(options);

        options.addOption(
            Poco::Util::Option("help", "h", "display help information")
                .required(false)
                .repeatable(false)
                .callback(Poco::Util::OptionCallback<CurrencyServerApp>(this, &CurrencyServerApp::handleHelp)));

        options.addOption(
            Poco::Util::Option("config", "c",
                                "path to the JSON configuration file (default: config/server.default.json)")
                .required(false)
                .repeatable(false)
                .argument("FILE")
                .binding("app.configFile"));

        options.addOption(
            Poco::Util::Option("define", "D", "override a config key, e.g. -Dcache.history.max_age_minutes=60")
                .required(false)
                .repeatable(true)
                .argument("KEY=VALUE")
                .callback(Poco::Util::OptionCallback<CurrencyServerApp>(this, &CurrencyServerApp::handleDefine)));
    }

    void handleHelp(const std::string& /*name*/, const std::string& /*value*/) {
        helpRequested_ = true;
        Poco::Util::HelpFormatter helpFormatter(options());
        helpFormatter.setCommand(commandName());
        helpFormatter.setUsage("[OPTIONS]");
        helpFormatter.setHeader("Currency Converter backend -- gRPC server for currency.CurrencyService.");
        helpFormatter.format(std::cout);
        stopOptionsProcessing();
    }

    // Backs the "-D/--define KEY=VALUE" option: writes straight into the
    // application's own (writeable, highest-priority) configuration layer,
    // so it always wins over the JSON file loaded later in main().
    void handleDefine(const std::string& /*name*/, const std::string& value) {
        const auto pos = value.find('=');
        if (pos == std::string::npos) {
            throw Poco::Util::OptionException("--define expects KEY=VALUE, got: " + value);
        }
        config().setString(value.substr(0, pos), value.substr(pos + 1));
    }

    // Console + rotating file, combined via SplitterChannel; level driven by
    // logging.level ("off" silences everything).
    void setupLogging(const currency::config::AppConfig& appConfig) {
        Poco::AutoPtr<Poco::ConsoleChannel> consoleChannel(new Poco::ConsoleChannel);

        Poco::AutoPtr<Poco::SplitterChannel> splitter(new Poco::SplitterChannel);
        splitter->addChannel(consoleChannel);

        const std::string logFile = appConfig.loggingFile();
        Poco::AutoPtr<Poco::FileChannel> fileChannel(new Poco::FileChannel(logFile));
        fileChannel->setProperty("rotation", "10 M");
        fileChannel->setProperty("archive", "timestamp");
        fileChannel->setProperty("compress", "true");
        try {
            fileChannel->open();
            splitter->addChannel(fileChannel);
        } catch (const Poco::Exception& ex) {
            // Missing/unwritable log directory shouldn't be fatal -- degrade
            // to console-only rather than crash the server over logging.
            std::cerr << "warning: could not open log file '" << logFile << "': " << ex.displayText()
                      << " -- logging to console only" << std::endl;
        }

        Poco::AutoPtr<Poco::PatternFormatter> formatter(
            new Poco::PatternFormatter("%Y-%m-%d %H:%M:%S.%i [%p] %s: %t"));
        Poco::AutoPtr<Poco::FormattingChannel> formattingChannel(new Poco::FormattingChannel(formatter, splitter));

        Poco::Logger::root().setChannel(formattingChannel);

        Poco::Logger::root().setLevel(toPocoLevel(appConfig.loggingLevel()));
    }

    int main(const std::vector<std::string>& /*args*/) override {
        if (helpRequested_) {
            return Application::EXIT_OK;
        }

        currency::config::AppConfig appConfig(config());
        const std::string configFile = config().getString("app.configFile", "config/server.default.json");
        if (Poco::File(configFile).exists()) {
            appConfig.loadJsonFile(configFile);
        } else {
            std::cerr << "warning: config file not found (" << configFile << "), using built-in defaults"
                      << std::endl;
        }

        setupLogging(appConfig);
        Poco::Logger& logger = Poco::Logger::get("main");
        logger.information("starting currency-server");

        Poco::Net::initializeSSL();

        currency::external::FrankfurterClient frankfurter(appConfig.externalApiBaseUrl(),
                                                            appConfig.externalApiTimeout());

        // Currency list changes essentially never; a fixed, generous TTL is
        // enough and doesn't need its own config surface.
        currency::cache::TimeAndSizeBoundedCache<std::vector<currency::domain::Currency>> currencyCache(
            "currencies", std::chrono::hours(24), 8);

        currency::cache::TimeAndSizeBoundedCache<currency::domain::ExchangeRate> latestRateCache(
            "latest_rate", appConfig.latestRateCacheMaxAge(), appConfig.latestRateCacheMaxElements());

        currency::cache::TimeAndSizeBoundedCache<std::vector<currency::domain::ExchangeRate>> historyCache(
            "history", appConfig.historyCacheMaxAge(), appConfig.historyCacheMaxElements());

        currency::application::CurrencyCatalogService catalog(frankfurter, currencyCache);
        currency::application::ConstraintsService constraints;
        currency::application::ConversionService conversion(frankfurter, latestRateCache, catalog, constraints);
        currency::application::HistoryService history(frankfurter, historyCache, catalog, constraints);

        currency::grpcapi::CurrencyServiceImpl service(catalog, conversion, history, constraints);

        // Lets `grpcurl -plaintext` introspect the service without needing
        // the .proto file on the client side -- convenient for the manual
        // verification this project relies on instead of unit tests.
        grpc::reflection::InitProtoReflectionServerBuilderPlugin();

        const std::string serverAddress = fmt::format("0.0.0.0:{}", appConfig.grpcPort());
        grpc::ServerBuilder builder;
        builder.AddListeningPort(serverAddress, grpc::InsecureServerCredentials());
        builder.RegisterService(&service);

        std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
        if (!server) {
            logger.error(fmt::format("failed to start gRPC server on {}", serverAddress));
            Poco::Net::uninitializeSSL();
            return Application::EXIT_SOFTWARE;
        }
        logger.information(fmt::format("gRPC server listening on {}", serverAddress));

        std::thread serverThread([&server] { server->Wait(); });

        waitForTerminationRequest();

        logger.information("shutdown requested, stopping gRPC server");
        server->Shutdown();
        serverThread.join();

        Poco::Net::uninitializeSSL();
        return Application::EXIT_OK;
    }

private:
    bool helpRequested_ = false;
};

POCO_SERVER_MAIN(CurrencyServerApp)
