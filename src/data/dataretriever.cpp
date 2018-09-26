//
// Created by Evan Kirkiles on 9/25/2018.
//

// Include header
#include "dataretriever.hpp"
// Global backtester namespace
namespace backtester {

// Constructor to build an instance of the DataRetriever for the given type of data.
//
// @param type             The type of data which will be used for this data retriever.
//                          -> HISTORICAL_DATA, INTRADAY_DATA, REALTIME_DATA
//
DataRetriever::DataRetriever(const std::string &p_type) {
    // First initialize the session options with global session run settings.
    BloombergLP::blpapi::SessionOptions session_options;
    session_options.setServerHost(bloomberg_session::HOST);
    session_options.setServerPort(bloomberg_session::PORT);
    // Also build the Event Handler to receive all incoming events

}

// Constructor for the Data Handler, simply saving the object to fill as a local pointer member.
HistoricalDataHandler::HistoricalDataHandler(std::unordered_map<std::string, SymbolHistoricalData> *p_target) :
        target(p_target) {}

// Data processing done using the request responses sent from Bloomberg API. In this case, historical data
// can be very large, so it may be split up into PARTIAL_RESPONSE objects instead of one RESPONSE. Either way,
// the data is interpreted and then returned as an Element object which contains the historical data requested.
// In case of an error, the object will not be filled.
bool HistoricalDataHandler::processEvent(const BloombergLP::blpapi::Event &event,
                                         BloombergLP::blpapi::Session *session) {

    // Iterates through the messages returned by the event
    BloombergLP::blpapi::MessageIterator msgIter(event);
    while (msgIter.next()) {
        // Get the message object
        BloombergLP::blpapi::Message msg = msgIter.message();
        // Make sure the message is either a partial response or a full response
        if ((event.eventType() != BloombergLP::blpapi::Event::PARTIAL_RESPONSE) &&
            (event.eventType() != BloombergLP::blpapi::Event::RESPONSE)) { continue; }

        // Make sure did not run into any errors
        if (!processExceptions(msg)) {
            if (!processErrors(msg)) {
                // Now process the fields and load them into the pointed to target object
                BloombergLP::blpapi::Element security_data = msg.getElement(element_names::SECURITY_DATA);
                // Build an empty SymbolHistoricalData for the symbol of the event
                SymbolHistoricalData shd;
                shd.symbol = security_data.getElementAsString(element_names::SECURITY_NAME);

                // Now get the field data and put it into the target map
                BloombergLP::blpapi::Element field_data = security_data.getElement(element_names::FIELD_DATA);
                if (field_data.numValues() > 0) {
                    for (int i = 0; i < field_data.numValues(); ++i) {
                        // Get the element and its date and put them into the SymbolHistoricalData object
                        BloombergLP::blpapi::Element element = field_data.getValueAsElement(static_cast<size_t>(i));
                        BloombergLP::blpapi::Datetime date = element.getElementAsDatetime(element_names::DATE);

                        // Put the information into the SymbolHistoricalData
                        shd.data[date] = element;
                    }
                }

                // Once all the data has been entered, append the SymbolHistoricalData to the target
                target->operator[](shd.symbol) = shd;
            } else {
                // Log that an error occurred for the event
                std::cout << "Error occurred! Cannot pull security data." << std::endl;
            }
        } else {
            // Log that an exception occurred for the event
            std::cout << "Exception occurred! Cannot pull security data." << std::endl;
        }
    }
}

// Handles any exceptions in the message received from Bloomberg.
bool HistoricalDataHandler::processExceptions(BloombergLP::blpapi::Message msg) {
    BloombergLP::blpapi::Element security_data = msg.getElement(element_names::SECURITY_DATA);
    BloombergLP::blpapi::Element field_exceptions = security_data.getElement(element_names::FIELD_EXCEPTIONS);

    // Ensure there are no field exceptions
    if (field_exceptions.numValues() > 0) {
        BloombergLP::blpapi::Element element = field_exceptions.getValueAsElement(0);
        BloombergLP::blpapi::Element field_id = element.getElement(element_names::FIELD_ID);
        BloombergLP::blpapi::Element error_info = element.getElement(element_names::ERROR_INFO);
        BloombergLP::blpapi::Element error_message = error_info.getElement(element_names::ERROR_MESSAGE);
        // Log the exceptions found to the console and return true for errors found
        std::cout << field_id << std::endl;
        std::cout << error_message << std::endl;
        return true;
    }
    return false;
}

// Handles any errors in the data received from Bloomberg.
bool HistoricalDataHandler::processErrors(BloombergLP::blpapi::Message msg) {
    BloombergLP::blpapi::Element security_data = msg.getElement(element_names::SECURITY_DATA);

    // If a security error occurred, notify the user
    if (security_data.hasElement(element_names::SECURITY_ERROR)) {
        BloombergLP::blpapi::Element security_error = security_data.getElement(element_names::SECURITY_ERROR);
        BloombergLP::blpapi::Element error_message = security_error.getElement(element_names::ERROR_MESSAGE);
        // Log the errors found to the console and return true for errors found
        std::cout << error_message << std::endl;
        return true;
    }
    return false;
}

}
