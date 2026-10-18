#include <elysia_text/catalog.hpp>
#include <elysia/elysia.hpp>
#include <algorithm>
#include <fstream>
#include <map>
#include <stdexcept>
#include <unordered_map>
#include <yaml-cpp/yaml.h>

namespace elysia::text {
namespace {
struct Hash {
    using is_transparent = void;
    size_t operator()(std::string_view key) const noexcept {
        return std::hash<std::string_view>{}(key);
    }
};
using Dictionary = std::unordered_map<std::string, std::string, Hash, std::equal_to<>>;
struct Translation {
    std::string name;
    Dictionary messages;
};
} // namespace
struct Catalog::State {
    std::string language = "en";
    std::map<std::string, Translation, std::less<>> translations{{"en", {"English", {}}}};
    Dictionary active;
    mutable Dictionary labels;
    uint64_t revision = 0;

    void resolve() {
        auto next = translations.at("en").messages;
        if (auto selected = translations.find(language); selected != translations.end())
            for (const auto& [key, value] : selected->second.messages)
                next[key] = value;
        active = std::move(next);
        labels.clear();
        ++revision;
    }
};
Catalog::Catalog() : state_(std::make_unique<State>()) {}
Catalog::~Catalog() = default;
void Catalog::load(const std::filesystem::path& directory) {
    if (!std::filesystem::exists(directory))
        return;
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(directory))
        if (entry.is_regular_file() &&
            (entry.path().extension() == ".yaml" || entry.path().extension() == ".yml"))
            files.push_back(entry.path());
    std::sort(files.begin(), files.end());
    // Stage the complete package, so an invalid later file cannot partially change live text.
    auto translations = state_->translations;
    for (const auto& file : files) {
        try {
            std::ifstream input(file, std::ios::binary);
            if (!input)
                throw std::runtime_error("Cannot open translation file");
            const auto document = YAML::Load(input);
            if (!document.IsMap())
                throw std::invalid_argument("Expected a string-to-string mapping");
            Dictionary messages;
            std::string name;
            for (const auto& item : document) {
                if (!item.first.IsScalar() || !item.second.IsScalar())
                    throw std::invalid_argument("Translation keys and values must be strings");
                auto key = item.first.as<std::string>();
                auto value = item.second.as<std::string>();
                if (key.empty() || key.find("###") != std::string::npos)
                    throw std::invalid_argument("Invalid translation key");
                if (!messages.emplace(key, value).second)
                    throw std::invalid_argument("Duplicate translation key: " + key);
            }
            if (auto found = messages.find("_name"); found != messages.end()) {
                name = std::move(found->second);
                messages.erase(found);
            }
            const auto locale = file.stem().string();
            auto& target = translations[locale];
            if (!name.empty())
                target.name = std::move(name);
            if (target.name.empty())
                target.name = locale;
            for (auto& [key, value] : messages)
                target.messages[key] = std::move(value);
        } catch (const std::exception& error) {
            throw std::runtime_error(file.string() + ": " + error.what());
        }
    }
    state_->translations = std::move(translations);
    state_->resolve();
}
void Catalog::select(std::string language) {
    if (language != state_->language) {
        state_->language = std::move(language);
        state_->resolve();
    }
}
std::string_view Catalog::language() const {
    return state_->language;
}
std::vector<Language> Catalog::languages() const {
    std::vector<Language> result;
    for (const auto& [id, value] : state_->translations)
        result.push_back({id, value.name});
    return result;
}
uint64_t Catalog::revision() const {
    return state_->revision;
}
const char* Catalog::get(Message message) const {
    if (!message.id || !*message.id)
        return message.fallback ? message.fallback : "";
    if (auto found = state_->active.find(message.id); found != state_->active.end())
        return found->second.c_str();
    return message.fallback ? message.fallback : message.id;
}
const char* Catalog::label(Message message) const {
    if (!message.id || !*message.id)
        throw std::invalid_argument("Localized widgets require a stable message ID");
    auto found = state_->labels.find(message.id);
    if (found == state_->labels.end())
        found = state_->labels.emplace(message.id, std::string(get(message)) + "###" + message.id)
                    .first;
    return found->second.c_str();
}
void Plugin::build(App& app) const {
    app.world().resources().get_or_create<Catalog>();
}
} // namespace elysia::text
