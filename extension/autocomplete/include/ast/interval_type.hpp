#pragma once
#include "duckdb/common/enums/date_part_specifier.hpp"
#include "duckdb/common/exception/parser_exception.hpp"

namespace duckdb {

class Interval {
	constexpr int32_t MONTH_MASK = 1 << 1;
	constexpr int32_t YEAR_MASK = 1 << 2;
	constexpr int32_t DAY_MASK = 1 << 3;
	constexpr int32_t HOUR_MASK = 1 << 10;
	constexpr int32_t MINUTE_MASK = 1 << 11;
	constexpr int32_t SECOND_MASK = 1 << 12;
	constexpr int32_t MILLISECOND_MASK = 1 << 13;
	constexpr int32_t MICROSECOND_MASK = 1 << 14;
	constexpr int32_t WEEK_MASK = 1 << 24;
	constexpr int32_t DECADE_MASK = 1 << 25;
	constexpr int32_t CENTURY_MASK = 1 << 26;
	constexpr int32_t MILLENNIUM_MASK = 1 << 27;
	constexpr int32_t QUARTER_MASK = 1 << 29;

	int32_t GetIntervalMask(DatePartSpecifier date_part) {
		switch (date_part) {
		case DatePartSpecifier::YEAR:
			return YEAR_MASK;
		case DatePartSpecifier::MONTH:
			return MONTH_MASK;
		case DatePartSpecifier::DAY:
			return DAY_MASK;
		case DatePartSpecifier::DECADE:
			return DECADE_MASK;
		case DatePartSpecifier::CENTURY:
			return CENTURY_MASK;
		case DatePartSpecifier::MILLENNIUM:
			return MILLENNIUM_MASK;
		case DatePartSpecifier::MICROSECONDS:
			return MICROSECOND_MASK;
		case DatePartSpecifier::MILLISECONDS:
			return MILLISECOND_MASK;
		case DatePartSpecifier::SECOND:
			return SECOND_MASK;
		case DatePartSpecifier::MINUTE:
			return MINUTE_MASK;
		case DatePartSpecifier::HOUR:
			return HOUR_MASK;
		case DatePartSpecifier::WEEK:
			return WEEK_MASK;
		case DatePartSpecifier::QUARTER:
			return QUARTER_MASK;
		case DatePartSpecifier::DOW:
		case DatePartSpecifier::ISODOW:
		case DatePartSpecifier::ISOYEAR:
		case DatePartSpecifier::DOY:
		case DatePartSpecifier::YEARWEEK:
		case DatePartSpecifier::ERA:
		case DatePartSpecifier::TIMEZONE:
		case DatePartSpecifier::TIMEZONE_HOUR:
		case DatePartSpecifier::TIMEZONE_MINUTE:
		case DatePartSpecifier::EPOCH:
		case DatePartSpecifier::JULIAN_DAY:
		case DatePartSpecifier::INVALID:
			throw ParserException("%s not supported as INTERVAL type", DatePartSpecifierToString(date_part));
		}
		throw InternalException("Unhandled DateSpecifierType");
	}
};
}
