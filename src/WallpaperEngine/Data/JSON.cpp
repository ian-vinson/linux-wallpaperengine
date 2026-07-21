#include "JSON.h"

#include "WallpaperEngine/Data/Parsers/UserSettingParser.h"
#include "WallpaperEngine/Logging/Log.h"

#include <exception>

using namespace WallpaperEngine::Data::JSON;
using namespace WallpaperEngine::Data::Model;
using namespace WallpaperEngine::Data::Parsers;

namespace {
// Drops commas that appear (ignoring whitespace) right before a closing ']' or '}', as long as
// they're outside a string literal. A single linear scan is enough -- each comma only needs to
// know what immediately follows it, not the surrounding bracket nesting.
std::string stripTrailingCommas (const std::string& text) {
    std::string result;
    result.reserve (text.size ());

    bool inString = false;
    bool escaped = false;

    for (size_t i = 0; i < text.size (); i++) {
	const char c = text[i];

	if (inString) {
	    result += c;

	    if (escaped) {
		escaped = false;
	    } else if (c == '\\') {
		escaped = true;
	    } else if (c == '"') {
		inString = false;
	    }

	    continue;
	}

	if (c == '"') {
	    inString = true;
	    result += c;
	    continue;
	}

	if (c == ',') {
	    size_t next = i + 1;

	    while (next < text.size () &&
		   (text[next] == ' ' || text[next] == '\t' || text[next] == '\r' || text[next] == '\n')) {
		next++;
	    }

	    if (next < text.size () && (text[next] == ']' || text[next] == '}')) {
		// drop this comma, it's a trailing one
		continue;
	    }
	}

	result += c;
    }

    return result;
}
} // namespace

JSON WallpaperEngine::Data::JSON::parseLenient (const std::string& text) {
    try {
	return JSON::parse (text);
    } catch (const nlohmann::json::parse_error&) {
	const auto originalError = std::current_exception ();
	const std::string repaired = stripTrailingCommas (text);

	if (repaired == text) {
	    std::rethrow_exception (originalError);
	}

	try {
	    JSON result = JSON::parse (repaired);
	    sLog.error ("JSON had trailing comma(s), tolerated after stripping them");
	    return result;
	} catch (const nlohmann::json::parse_error&) {
	    // stripping trailing commas didn't fix it either -- some other error, surface the
	    // original diagnostic (its line/column match the real file, not the repaired copy)
	    std::rethrow_exception (originalError);
	}
    }
}

UserSettingUniquePtr JsonExtensions::user (const std::string& key, const Properties& properties) const {
    const auto value = this->require (key, "User setting without default value must be present");

    return UserSettingParser::parse (value, properties);
}

UserSettingUniquePtr JsonExtensions::color (const std::string& key, const Properties& properties) const {
    const auto value = this->require (key, "User setting without default value must be present");

    return UserSettingParser::parse (value, properties, true);
}
