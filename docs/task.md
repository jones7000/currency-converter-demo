# Currency Converter

The goal is to build a client-server application that displays currency information, performs currency conversions, and visualizes historical exchange rate trends.

Split into two applications:
- Web Application (Client): A frontend application responsible for rendering UI components and managing user interaction.
- Server Application (Backend): A backend responsible for communicating with the external public API, caching results, and serving data to the client.
- **Writing unit tests are not required and can be omitted.**

## Web Application (Frontend)
* The frontend must be built using the Angular framework, using material framework.
* You are free to use any third-party UI or charting libraries. 
* The web application must act as a "dumb client."
* Avoid any business logic computations on the frontend; all logic should be handled by the backend server.

### The Web Application must feature two main sections:
* Part A: Currency Overview
    * Display a list or table of all available currencies.
    * For each currency, show its ISO code and full name. 
* Part B: Currency Conversion and Historical Trends
    * The UI must include:
        * Selectors for a source currency and a target currency.
        * An input field for an amount to convert (define useful min max amount borders).
        * Date selectors to define the start and end date for a historical timeframe (matches the min max dates, borders).
    * Upon user submission, the client will query the backend server. The frontend must then display:
        * The converted amount, alongside the date of the latest rate is used.
        * A visual plot (chart/graph) showing the historical exchange rates between the selected currencies over the chosen timeframe.

## Server Application (Backend)
* The backend must be written in C++.
* Use POCO or Boost as the external library for the HTTP Client/Server, which fits best? (> has to be evaluated)
* It is responsible for querying the public API https://frankfurter.dev to retrieve currency and conversion rates.
* To minimize redundant API calls, you must implement a caching mechanism on the backend:
    * All queries for historical exchange rates must be cached. 
    * If the server receives a repeated query for data that is already cached, it must serve the response from the local cache instead of making a new request to the external API.
* Caching Strategies:
    * The backend cache must implement using two strategies:
        * Time-Based: The cache entries have a configurable maximum age (max_age in minutes/days). Any cache entry older than this limit must be automatically removed.
        * Size-Based: The cache must have a configurable capacity limit (max_elements). If adding a new element exceeds this limit, the oldest entry must be deleted.
        * Both configuration parameters max_age and max_elements must be loaded from a configuration file on the filesystem (JSON) or from application arguments (e.g. int main(int argc, char** argv)).

## Communication & Integration
Protocol: Choose between gRPC or a REST API for communication between the Web Application and the Server Application.
API Contract: Use Protocol Buffers (.proto files) for the data models and communication contracts, regardless of whether you choose gRPC or REST. The template is provided; you are free to extend or modify. 

```
syntax = "proto3";
package currency;

message CurrencyInfo {
    string code = 1;
    string full_name = 2;
}

message ConvertRequest {
    string source_currency = 1;
    string target_currency = 2;
    double amount = 3;
}


message ConvertResponse {
    double source_amount = 1;
    double converted_amount = 2;
    double exchange_rate = 3;
    string rate_date = 4;
}

message HistoryRequest {
    string source_currency = 1;
    string target_currency = 2;
    string start_date = 3;
    string end_date = 4;
}

message HistoryPoint {
    string date = 1;
    double rate = 2;
}

message HistoryResponse {
    string source_currency = 1;
    string target_currency = 2;
    repeated HistoryPoint points = 3;
}
```