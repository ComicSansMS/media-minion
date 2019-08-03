#include <media_minion/player/configuration.hpp>

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

namespace media_minion::player {

std::optional<Configuration> parsePlayerConfig(std::filesystem::path const& config_filepath)
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

    if (!config_doc.HasMember("server") || !config_doc["server"].IsObject()) {
        GHULBUS_LOG(Error, "Invalid value for option 'server'");
        return std::nullopt;
    }

    auto const config_server = config_doc["server"].GetObject();
    if (!config_server.HasMember("hostname") || !config_server["hostname"].IsString()) {
        GHULBUS_LOG(Error, "Invalid value for option 'server.hostname'");
        return std::nullopt;
    }
    config.server_host = config_server["hostname"].GetString();

    if (!config_server.HasMember("port") || !config_server["port"].IsNumber()) {
        GHULBUS_LOG(Error, "Invalid value for option 'server.port'");
        return std::nullopt;
    }
    auto const port_number = config_server["port"].GetInt();
    if (port_number < 0 || port_number > 65535) {
        GHULBUS_LOG(Error, "Port number in config file " << config_filepath << " out of range: " << port_number);
        return std::nullopt;
    }
    config.server_port = static_cast<std::uint16_t>(port_number);

    return config;
}

}

