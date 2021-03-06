//
// Created by Evan Kirkiles on 1/25/2019.
//

// Include corresponding header
#include "strategy/custom/src/momentum1.hpp"

// Initialize the strategy to backtest
ALGO_Momentum1::ALGO_Momentum1(const BloombergLP::blpapi::Datetime &start, const BloombergLP::blpapi::Datetime &end,
                     unsigned int capital) :
        Strategy({"DIA US EQUITY", "QQQ US EQUITY", "LQD US EQUITY",
                  "HYG US EQUITY", "USO US EQUITY", "GLD US EQUITY",
                  "VNQ US EQUITY", "RWX US EQUITY", "UNG US EQUITY",
                  "DBA US EQUITY"},
                 capital,
                 start,
                 end,
                 R"(C:\Users\bloomberg\CLionProjects\bloomberg_backtester\saves\state1.txt)") {

    // Preload the data so do not have to do so many Bloomberg API requests
    log("Pulling entire backtest data.");
    dynamic_cast<HistoricalDataManager*>(data.get())->preload(symbol_list, {"PX_OPEN", "PX_LAST"}, start, end, 127);
    log("Backtest data pull complete.");

    // Prepare performance csv
    std::ofstream output;
    output.open(R"(C:\Users\bloomberg\CLionProjects\bloomberg_backtester\saves\performance.csv)", std::ios_base::trunc | std::ios_base::out);
    output.close();

    // Perform constant declarations and definitions here.
    context["lookback"] = 126;                                // The lookback for the moving average
    context["maxleverage"] = 0.9;                             // The maximum leverage allowed
    context["multiple"] = 2;                                  // 1% of return translates to what weight? e.g. 5%
    context["profittake"] = 1.96;                             // Take profits when breaks out of 95% Bollinger Band
    context["slopemin"] = 0.252;                              // Minimum slope on which to be trading on
    context["dailyvolatilitytarget"] = 0.025;                 // Target daily volatility, in percent

    // Dictionary of weights, set for each security
    for (const std::string& sym : symbol_list) {
        symbolspecifics[sym]["weight"] = 0.0;
        symbolspecifics[sym]["bought"] = 1;
        symbolspecifics[sym]["stopprice"] = -100000000;
    }

    // Perform function scheduling here.
    // This lambda is annoying but necessary for downcasting the Strategy to the type of your algorithm when
    // it is known, otherwise cannot have schedule_function declared in Strategy base class.

    // Every 11 minutes during market hours
    // 1. Checks for any exit conditions in securities where weight != 0
    schedule_function([](Strategy* x)->void { auto a = dynamic_cast<ALGO_Momentum1*>(x); if (a) a->exitconditions(); },
                      date_rules.every_day(), TimeRules::every_minute(10));

    // 28 minutes after market opens
    // 2. Performs the regression and calculates the weights for any new trends
    schedule_function([](Strategy* x)->void { auto b = dynamic_cast<ALGO_Momentum1*>(x); if (b) b->regression(); },
                      date_rules.every_day(), TimeRules::market_open(0, 28));

    // 30 minutes after market opens
    // 3. Performs trades and notifies us of any required buys/sells
    schedule_function([](Strategy* x)->void { auto c = dynamic_cast<ALGO_Momentum1*>(x); if (c) c->trade(); },
                      date_rules.every_day(), TimeRules::market_open(0, 30));

    // At close of every day
    // 4. Reports performance of portfolio
    schedule_function([](Strategy* x)->void { auto d = dynamic_cast<ALGO_Momentum1*>(x); if (d) d->reportperformance(); },
                      date_rules.every_day(), TimeRules::market_close(0, 0));
}

// Checks for new trends available to go in on. Conditions:
//  1. Normalized slope over past [lookback] days is greater than [minslope]
//  2. Price has crossed the regression line.
void ALGO_Momentum1::regression() {
    // Pull the past [lookback] of data from Bloomberg
    std::unique_ptr<std::unordered_map<std::string, SymbolHistoricalData>> prices =
            data->history(symbol_list, {"PX_OPEN"}, (unsigned int) std::ceil(context["lookback"]*1.6), "DAILY");

    // Iterate through each symbol to perform logic for each
    for (const std::string& symbol : symbol_list) {
        // First build the vector for which regression can be calculated, along with a sd vairable
        std::vector<double> x;
        double sum_x1=0, sum_x2=0;
        auto iter = prices->at(symbol).data.begin();
        for (std::advance(iter, (prices->at(symbol).data.size() - (int)context["lookback"]));
                iter != prices->at(symbol).data.end(); iter++) {
            double val = (*iter).second["PX_OPEN"];
            x.emplace_back(val);
            sum_x1 += val;
            sum_x2 += val * val;
        }

        // Get the price series fo
        // With the price vector built, perform the regression
        std::pair<double, double> results = calcreg(x);
        // Get the normalized slope (return per year)
        const double slope = results.first / results.second * 252.0;
        // Calculate the difference in actual price vs regression over the past 2 days
        auto priceiter = prices->at(symbol).data.rbegin();
        const double delta1 = (*priceiter).second["PX_OPEN"] - (results.first*context["lookback"] + results.second);
        priceiter++;
        const double delta2 = (*priceiter).second["PX_OPEN"] - (results.first*(context["lookback"]-1) + results.second);
        // Also get the standard deviation of the price series
        const double sd = sqrt((sum_x2 / context["lookback"]) - ((sum_x1 / context["lookback"]) * (sum_x1 / context["lookback"])));

        // If long but the slope turns down, exit
        if (symbolspecifics[symbol]["weight"] > 0 && slope < 0) {
            symbolspecifics[symbol]["weight"] = 0.0;
            symbolspecifics[symbol]["bought"] = 0;
            log("v Slope turned bull " + symbol);
        // If shortbut the slope turns up, exit
        } else if (symbolspecifics[symbol]["weight"] < 0 && slope > 0) {
            symbolspecifics[symbol]["weight"] = 0.0;
            symbolspecifics[symbol]["bought"] = 0;
            log("v Slope turned bear " + symbol);
        }

        // If the trend is up enough
        if (slope > context["slopemin"]) {
            // If the price crosses the regression line and not already in a position for the stock
            if (delta1 > 0 && delta2 < 0 && symbolspecifics[symbol]["weight"] == 0) {
                // Set the weight to be ordered at the end of the day, and clear the stopprice
                symbolspecifics[symbol]["stopprice"] = -100000000;
                symbolspecifics[symbol]["weight"] = slope;
                symbolspecifics[symbol]["bought"] = 0;
                log(std::string("---------- Long  a = ") + std::to_string(slope * 100) + "% for " + symbol);
            // If the price is greater than the profit take Bollinger Band and we are long in it
            } else if (delta1 > context["profittake"] * sd && symbolspecifics[symbol]["weight"] > 0) {
                // Exit the position by setting the weight to 0
                symbolspecifics[symbol]["weight"] = 0.0;
                symbolspecifics[symbol]["bought"] = 0;
                log("---- Exit long in " + symbol);
            }
        // If the trend is down enough
        } else if (slope < -context["slopemin"]) {
            // If the price crosses the regression line and not already in a position for the stock
            if (delta1 < 0 && delta2 > 0 && symbolspecifics[symbol]["weight"] == 0) {
                // Set the weight to be ordered at the end of the day, and clear the stopprice
                symbolspecifics[symbol]["stopprice"] = -100000000;
                symbolspecifics[symbol]["weight"] = slope;
                symbolspecifics[symbol]["bought"] = 0;
                log(std::string("---------- Short  a = ") + std::to_string(slope * 100) + "% for " + symbol);
                // If the price is less than the profit take Bollinger Band and we are short in it
            } else if (delta1 < -context["profittake"] * sd && symbolspecifics[symbol]["weight"] < 0) {
                // Exit the position by setting the weight to 0
                symbolspecifics[symbol]["weight"] = 0.0;
                symbolspecifics[symbol]["bought"] = 0;
                log("---- Exit short in " + symbol);
            }
        }
    }
}

// Exits positions where the trend seems to be fading. Conditions:
//  1. Stop price is hit (the estimated price before the lookback period according to the regression)
void ALGO_Momentum1::exitconditions() {
    // Again, iterate through the symbols to perform the calculations for each stock
    for (const std::string& symbol : symbol_list) {
        // Get the mean price over the past couple of days as a more robust statistic
        std::unique_ptr<std::unordered_map<std::string, SymbolHistoricalData>> prices =
                data->history(symbol_list, {"PX_LAST"}, 4, "DAILY");
        double price=0;
        double n = 0;
        for (auto &iter : prices->at(symbol).data) {
            n++;
            price += iter.second["PX_LAST"];
        }

        price /= n;

        // Now calculate the stop loss percentage as the estimated return over the lookback
        double stoploss = std::abs(symbolspecifics[symbol]["weight"] * context["lookback"] / 252.0) + 1;

        // Check if we should exit depending on whether we are long or short
        if (symbolspecifics[symbol]["weight"] > 0) {
            // If the stop price is negative, we need to recalculate it
            if (symbolspecifics[symbol]["stopprice"] == -100000000 || symbolspecifics[symbol]["stopprice"] < 0) {
                symbolspecifics[symbol]["stopprice"] = price / stoploss;
            // Otherwise, check the current price against the stop price
            } else {
                // Only update the stop price if it has increased so we are safer and do not have a low stop price.
                symbolspecifics[symbol]["stopprice"] = std::max(price/stoploss, symbolspecifics[symbol]["stopprice"]);
                // Make sure the price is outside of the stop price anyways, if not then sell all shares
                if (price < symbolspecifics[symbol]["stopprice"]) {
                    // Notify us of the exiting of the position
                    log("x Long stop loss for " + symbol+ ", sell all shares.");
                    symbolspecifics[symbol]["weight"] = 0;
                    symbolspecifics[symbol]["bought"] = 0;
                    // We just use order percent here because we want to exit the trend immediately (not end of day)
                    order_target_percent(symbol, 0);
                }
            }
        } else if (symbolspecifics[symbol]["weight"] < 0) {
            // If the stop price is negative, we need to recalculate it
            if (symbolspecifics[symbol]["stopprice"] == -100000000 || symbolspecifics[symbol]["stopprice"] < 0) {
                symbolspecifics[symbol]["stopprice"] = price / stoploss;
                // Otherwise, check the current price against the stop price
            } else {
                // Only update the stop price if it has decreased so we are safer and do not have a high stop price.
                symbolspecifics[symbol]["stopprice"] = std::min(price*stoploss, symbolspecifics[symbol]["stopprice"]);
                // Make sure the price is outside of the stop price anyways, if not then sell all shares
                if (price > symbolspecifics[symbol]["stopprice"]) {
                    // Notify us of the exiting of the position
                    log("x Short stop loss for " + symbol+ ", sell all shares.");
                    symbolspecifics[symbol]["weight"] = 0;
                    symbolspecifics[symbol]["bought"] = 0;
                    // We just use order percent here because we want to exit the trend immediately (not end of day)
                    order_target_percent(symbol, 0);
                }
            }
        } else {
            symbolspecifics[symbol]["stopprice"] = -100000000;
        }
    }
}

// Performs trades based on the weights in our symbolspecifics object.
// Note the volatility scalar which changes our positions based on how volatile the asset is.
void ALGO_Momentum1::trade() {
    // First, calculate the volatility scalar
    // std::unordered_map<std::string, double> vol_mult = volatilityscalars();
    // Also keep track of how many securities we do not have a position in
    int nopositions = 0;
    for (const std::string& sym : symbol_list) { if (symbolspecifics[sym]["weight"] != 0 ||
        (symbolspecifics[sym]["bought"] == 0 && symbolspecifics[sym]["weight"] == 0)) { nopositions++; } }

    // Now iterate through the weights and perform the trades
    for (const std::string& symbol : symbol_list) {
        // Only check if the symbol hasn't been bought yet
        if (symbolspecifics[symbol]["bought"] == 0) {
            if (symbolspecifics[symbol]["weight"] == 0) {
                // Log the exiting of the position
                order_target_percent(symbol, 0);
                symbolspecifics[symbol]["bought"] = 1;
            } else if (symbolspecifics[symbol]["weight"] > 0) {
                double percent =
                        (std::fmin(symbolspecifics[symbol]["weight"] * context["multiple"], context["maxleverage"]) /
                         nopositions); // * vol_mult[symbol];
                if (std::isnan(percent)) {
                    std::cout << "NaN trade value, skipping." << std::endl;
                    return;
                }
                log(std::string("^ Go long ") + std::to_string(percent*100) + "% in " + symbol);
                order_target_percent(symbol, percent);
                symbolspecifics[symbol]["bought"] = 1;
            } else if (symbolspecifics[symbol]["weight"] < 0) {
                double percent =
                        (std::fmax(symbolspecifics[symbol]["weight"] * context["multiple"], -context["maxleverage"]) /
                         nopositions); // * vol_mult[symbol];
                if (std::isnan(percent)) {
                    std::cout << "NaN trade value, skipping." << std::endl;
                    return;
                }
                log(std::string("v Go short ") + std::to_string(percent*100) + "% in " + symbol);
                order_target_percent(symbol, percent);
                symbolspecifics[symbol]["bought"] = 1;
            }
        }
    }
}

// Performs a linear regression to get the slope and intercept of a line for a given vector
// First element of tuple is the slope, second is the intercept.
std::pair<double, double> ALGO_Momentum1::calcreg(std::vector<double> x) {
    double xSum=0, ySum=0, xxSum=0, xySum=0, slope=0, intercept=0;
    // Build a vector of increasing-by-one integers to act as the Y array
    std::vector<int> y(x.size());
    std::iota(std::begin(y), std::end(y), 0);
    // Now find the slope and intercept
    for (int i = 0; i < y.size(); i++) {
        xSum += y[i];
        ySum += x[i];
        xxSum += y[i] * y[i];
        xySum += y[i] * x[i];
    }
    slope = (y.size()*xySum - xSum*ySum) / (y.size()*xxSum - xSum*xSum);
    intercept = (xxSum*ySum - xySum*xSum) / (y.size()*xxSum - xSum*xSum);
    return std::make_pair(slope, intercept);
}

// Reports the performance of the algorithm at end of every day.
void ALGO_Momentum1::reportperformance() {
    // Log the portfolio status
    log(std::string("Return: ") + std::to_string(portfolio.current_holdings[portfolio_fields::EQUITY_CURVE]) +
        std::string(", Value: ") + std::to_string(portfolio.current_holdings[portfolio_fields::TOTAL_HOLDINGS]) +
        std::string(", Held Cash: ") + std::to_string(portfolio.current_holdings[portfolio_fields::HELD_CASH]));

    // Build a .csv with the returns for plotting in Python
    std::ofstream output;
    output.open(R"(C:\Users\bloomberg\CLionProjects\bloomberg_backtester\saves\performance.csv)", std::ios_base::app | std::ios_base::out);
    output << portfolio.current_holdings[portfolio_fields::EQUITY_CURVE] << "\n";
    output.close();
}