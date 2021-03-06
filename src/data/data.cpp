//
// Created by Evan Kirkiles on 9/27/2018.
//

// Include the correlated header
#include <include/data.hpp>

#include "data.hpp"

// Constructor that sets up the connection to the Bloomberg Data API so data can be pulled.
HistoricalDataManager::HistoricalDataManager(BloombergLP::blpapi::Datetime* p_currentTime, int p_correlation_id) :
        DataManager(p_currentTime), dr("HISTORICAL_DATA", p_correlation_id) {}

// Function that builds the Market Events and puts them onto the HEAP event list in chronological order. Does so
// by first pulling the EOD last price data for the securities to be traded by the algorithm and then generating Market
// Events for each date. Goes through each symbol's data one entry at a time at the same time, comparing in case of
// dates not lining up and then catching up the iterators.
void HistoricalDataManager::fillHistory(const std::vector<std::string> &symbols,
                                        const BloombergLP::blpapi::Datetime& start,
                                        const BloombergLP::blpapi::Datetime& end,
                                        std::list<std::unique_ptr<events::Event>>* location) {

    // First retrieve the array of daily end of date prices
    std::unique_ptr<std::unordered_map<std::string, SymbolHistoricalData>> data =
            dr.pullHistoricalData(symbols, start, end);

    // Build an iterator off of the first stock's data to get all dates. Then, we can use the dates to build
    // packaged MarketEvents of multiple symbol data.
    // Potential data issues: what happens when a stock started trading after start date? Ends before end date?
    // Also, I don't know how Bloomberg handles missing values, which will be a weird occurrence when it shows up.
    for (auto i = data->operator[](symbols[0]).data.begin(); i != data->operator[](symbols[0]).data.end(); ++i) {
        // Placeholder data unordered map which will contain the update information
        std::unordered_map<std::string, double> temp = {{symbols[0], i->second["PX_LAST"]}};
        for (int j = 1; j < symbols.size(); ++j) {
            temp[symbols[j]] = data->operator[](symbols[j]).data[i->first]["PX_LAST"];
        }

        // Now build the Market Event and place it onto the HEAP as a unique ptr
        location->emplace_back(std::make_unique<events::MarketEvent>(symbols, temp, i->first));
    }
}

// Pulls history data from Bloomberg for a specified number of days before the current date, at a given frequency for
// set securities and fields. Cannot simply use the data downloaded to build the MarketEvents because that data only
// contains the last price (PX_LAST) when the algorithm may require other types.
std::unique_ptr<std::unordered_map<std::string, SymbolHistoricalData>> HistoricalDataManager::history(
            const std::vector<std::string> &symbols, const std::vector<std::string> &fields, unsigned int timeunitsback,
            const std::string &frequency) {

    // Find the date N days back from the current dates
    BloombergLP::blpapi::Datetime beginDate = date_funcs::add_seconds(*currentTime, 24 * 60 * 60 * timeunitsback * -1);

    if (!preloaded) {
        // Simulate a default argument for overridden function
        std::string freq = (frequency == "RECENT") ? "DAILY" : frequency;
        // Simply tunnels the request through to the HistoricalDataRetriever, filling in the end date as the current date of
        // the local pointer to the simulated current date.
        return std::move(dr.pullHistoricalData(symbols, beginDate, *currentTime, fields, freq));
    } else {
        // Temporary object to return
        std::unique_ptr<std::unordered_map<std::string, SymbolHistoricalData>> toReturn =
                std::make_unique<std::unordered_map<std::string, SymbolHistoricalData>>();
        // Have to iterate through all the SymbolHistoricalData objects and trim copies of them to the requested dates and fields.
        for(const std::string& symb : symbols) {
            (*toReturn)[symb] = preloaded_data->at(symb).trim(beginDate, *currentTime);
        }
        return std::move(toReturn);
    }
}

// Pulls history data from Bloomberg for the entire backtest which will then be used by the history function.
void HistoricalDataManager::preload(const std::vector<std::string> &symbols, const std::vector<std::string> &fields,
                                    const BloombergLP::blpapi::Datetime &start,
                                    const BloombergLP::blpapi::Datetime &end, unsigned int maxlookback,
                                    const std::string &frequency) {

    // Find the earliest possible data to be requested
    BloombergLP::blpapi::Datetime beginDate = date_funcs::add_seconds(start, 24 * 60 * 60 * maxlookback * -1);
    // Beginning at the found date, pull the historical data into the container
    preloaded_data = std::move(dr.pullHistoricalData(symbols, beginDate, end, fields, frequency));
    preloaded = true;
}
