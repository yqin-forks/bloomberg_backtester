//
// Created by Evan Kirkiles on 9/27/2018.
//

#ifndef BACKTESTER_EVENTS_HPP
#define BACKTESTER_EVENTS_HPP

// Include bloomberg includes
#include "bloombergincludes.hpp"

namespace events {

// Declaration of all Event types to be used in backtesting. This includes:
//   - ScheduledEvent  : specifies a function used in the strategy and schedules it on HEAP event list
//   - MarketEvent     : built by the data handler and specifies a price update to update holdings
//   - SignalEvent     : produced by the strategy and requests a fill for a security for a given percentage of holdings
//   - OrderEvent      : produced by the execution handler after reformatting a signal event to quantity
//   - FillEvent       : notifies the portfolio that an order has been filled, contains simulated commission & slippage

// Parent Event class which contains members used by all Events
//
// @member type           The type of the event this is
// @member datetime       The simulated datetime of the event used for putting onto the STACK
//
struct Event {
    const std::string type;
    const BloombergLP::blpapi::Datetime datetime = BloombergLP::blpapi::Datetime(1970, 1, 1, 0, 0, 0);

    // Concise one line print which tells what type the event is and its date
    void concise_what();
    // Print function to be implemented by the events themselves for debugging purposes
    virtual void what() = 0;
    // Default destructor to allow for polymorphism (so we can downcast sub event types from pointers to Events)
    virtual ~Event() = default;

    // For some reason error pops up when using Event object members, so need protected constructor
protected:
    Event(std::string p_type, const BloombergLP::blpapi::Datetime& p_datetime) :
            type(std::move(p_type)), datetime(p_datetime) {};
};

// MarketEvent that is produced by the DataManager class when the strategy is initialized. These are added onto the
// event HEAP with set dates, so the portfolio can cycle through the market information and update the holdings
// on a specified frequency. Scheduled events will be scattered in between as well as some OrderEvents.
//
// @member symbols          The symbols for which the market is providing a price update.
// @member data             An unordered map of the new price updates wrt their symbols.
//
struct MarketEvent : public Event {
    const std::vector<std::string> symbols;
    const std::unordered_map<std::string, double> data;

    // Print function
    void what() override;

    // Constructor for the MarketEvent
    MarketEvent(const std::vector<std::string> &symbols, const std::unordered_map<std::string, double> &data,
                const BloombergLP::blpapi::Datetime &when);
};

// SignalEvent which is produced when the algorithm requests an order. This acts as a middleman between the algorithm
// and the portfolio, producing an order event for a given quantity of stock based on the percentage of the holdings
// requested to be put into the given security.
//
// @member symbol            The symbol of the security being requested
// @member percentage        The target percentage of the holdings to be filled (positive for long, negative for short)
//
struct SignalEvent : public Event {
    const std::string symbol;
    const double percentage;

    // Print function
    void what() override;

    // Constructor for the SignalEvent
    SignalEvent(const std::string &symbol, double percentage, const BloombergLP::blpapi::Datetime& when);
};

// OrderEvent which is produced when a signal from teh algorithm is received by the execution handler. It may be
// split into multiple orders if the full order cannot be filled in a single bar. The slippage and commission is
// also handled in the production of this event. OrderEvents can be placed on both the STACK and the HEAP, where
// the STACK will perform them as soon as possible before any HEAP event is examined.
//
// @member symbol             The symbol of the security being ordered
// @member quantity           The quantity of the security being ordered (not const because mutable by execution handler)
//
struct OrderEvent : public Event {
    const std::string symbol;
    int quantity;

    // Print function
    void what() override;

    // Constructor for the OrderEvent
    OrderEvent(const std::string& symbol, int quantity, const BloombergLP::blpapi::Datetime& when);
};

// FillEvent which is produced upon a successful OrderEvent. All slippage and risk management will have been handled
// by the execution handler, so this event simply contains the cost and the quantity filled of the order.
//
// @member symbol             The symbol for which the signal is being sent
// @member quantity           The number of shares filled (positive for bought, negative for sold)
// @member cost               The price of the fill not including slippage or commission
// @member slippage           The cost of the slippage due to simulated variations in bid/ask spread
// @member commission         The cost of the commission given by the simulated broker (IB for now)
//
struct FillEvent : public Event {
    const std::string symbol;
    const int quantity;
    const double cost, slippage, commission;

    // Print function
    void what() override;

    // Constructor for the FillEvent
    FillEvent(const std::string &symbol, int quantity, double cost, double slippage, double commission,
            const BloombergLP::blpapi::Datetime& when);
};

// StopEvent which terminates the execution loop safely. Put on the stack so it breaks the while loop that interprets
// events, and allows the program to do any cleanup and post-run logging.
//
// @member reason              Why is the loop being stopped?
//
struct StopEvent : public Event {
    const std::string reason;

    // Print function
    void what() override;

    // Constructor for the StopEvent
    StopEvent(const std::string& reason, const BloombergLP::blpapi::Datetime& when);
};
}

#endif //BACKTESTER_EVENTS_HPP
