//
// Created by bloomberg on 9/25/2018.
//

// Include the declarations
#include "constants.hpp"

// Definition of global constants, each in their respective namespaces.
namespace bloomberg_session {
    const char* HOST("localhost");
    const uint16_t PORT(8194);
}

// Services available through the Bloomberg APi
namespace bloomberg_services {
    const char* REFDATA("//blp/refdata");
    const char* MKTDATA("//blp/mktdata");
}

// Definition of element-related constants for accessing Bloomberg data
namespace element_names {
    const char* SECURITY_DATA("securityData");
    const char* SECURITY_NAME("security");
    const char* DATE("date");

    const char* FIELD_ID("fieldId");
    const char* FIELD_DATA("fieldData");
    const char* FIELD_DESC("description");
    const char* FIELD_INFO("fieldInfo");
    const char* FIELD_ERROR("fieldError");
    const char* FIELD_MSG("message");
    const char* SECURITY_ERROR("securityError");
    const char* ERROR_MESSAGE("message");
    const char* FIELD_EXCEPTIONS("fieldExceptions");
    const char* ERROR_INFO("errorInfo");
    const char* RESPONSE_ERROR("responseError");
    const char* CATEGORY("category");
    const char* SUBCATEGORY("subcategory");
    const char* MESSAGE("message");
    const char* CODE("code");
}

// Names for data in the portfolio
namespace portfolio_fields {
    const char* HELD_CASH("held_cash");
    const char* COMMISSION("commission");
    const char* SLIPPAGE("slippage");
    const char* TOTAL_HOLDINGS("total_holdings");
    const char* RETURNS("returns");
    const char* EQUITY_CURVE("equity_curve");
}

// Enums for the date rules specifying types
namespace date_time_enums {
    const unsigned int EVERY_DAY = 0;
    const unsigned int WEEK_START = 1;
    const unsigned int WEEK_END = 2;
    const unsigned int MONTH_START = 3;
    const unsigned int MONTH_END = 4;

    const unsigned int T_MARKET_OPEN = 0;
    const unsigned int T_MARKET_CLOSE = 1;
    const unsigned int T_EVERY_MINUTE = 2;
    const unsigned int T_EVERY_MINUTE_OF_DAY = 3;

    const unsigned int US_MARKET_OPEN_HOUR = 9;
    const unsigned int US_MARKET_OPEN_MINUTE = 30;
    const unsigned int US_MARKET_CLOSE_HOUR = 16;
    const unsigned int US_MARKET_CLOSE_MINUTE = 0;

    const unsigned int US_MARKET_EARLY_CLOSE_HOUR = 13;
    const unsigned int US_MARKET_EARLY_CLOSE_MINUTE = 0;
}

namespace simulation {
    const double SLIPPAGE_LN_MEAN(0);
    const double SLIPPAGE_LN_SD(1);
}

namespace correlation_ids {
    const unsigned int HISTORICAL_REQUEST_CID(0);
    const unsigned int LIVE_REQUEST_CID(1);
    const unsigned int INTRADAY_REQUEST_CID(2);
}