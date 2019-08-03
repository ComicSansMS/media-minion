#include <media_minion/server/configuration.hpp>

#include <rapidjson/rapidjson.h>
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 5054)
#endif
#include <rapidjson/document.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <gbBase/Assert.hpp>
#include <gbBase/Log.hpp>

#include <fstream>

namespace media_minion::server {

std::optional<Configuration> parseServerConfig(std::filesystem::path const& config_filepath)
{
    GHULBUS_PRECONDITION(!config_filepath.empty());
    std::ifstream fin(config_filepath, std::ios_base::binary);
    if (!fin) {
        GHULBUS_LOG(Error, "Unable to open config file " << config_filepath);
        return std::nullopt;
    }

    fin.seekg(0, std::ios_base::end);
    std::size_t const filesize = fin.tellg();
    fin.seekg(0, std::ios_base::beg);

    std::vector<char> file_contents(filesize + 1, '\0');
    fin.read(file_contents.data(), filesize);
    if (!fin) {
        GHULBUS_LOG(Error, "Error while reading from config file " << config_filepath);
        return std::nullopt;
    }

    rapidjson::Document config_doc;
    config_doc.ParseInsitu(file_contents.data());
    rapidjson::ParseErrorCode const parse_error = config_doc.GetParseError();
    if (parse_error != rapidjson::kParseErrorNone) {
        GHULBUS_LOG(Error, "Error parsing json from config file " << config_filepath << ": " << parse_error);
        return std::nullopt;
    }

    Configuration config;

    if (!config_doc.HasMember("port") || !config_doc["port"].IsNumber()) {
        GHULBUS_LOG(Error, "Invalid value for option 'port'");
        return std::nullopt;
    }
    auto port_number = config_doc["port"].GetInt();
    if (port_number < 0 || port_number > 65535) {
        GHULBUS_LOG(Error, "Port number in config file " << config_filepath << " out of range: " << port_number);
        return std::nullopt;
    }
    config.listening_port = static_cast<std::uint16_t>(port_number);

    if (!config_doc.HasMember("useIPv6") || !config_doc["useIPv6"].IsBool()) {
        GHULBUS_LOG(Error, "Invalid value for option 'useIPv6'");
        return std::nullopt;
    }
    config.protocol = config_doc["useIPv6"].GetBool() ?
                      Configuration::Protocol::ipv6 :
                      Configuration::Protocol::ipv4;

    return config;
}

}

