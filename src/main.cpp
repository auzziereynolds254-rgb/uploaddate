#include <Geode/Geode.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>

#include <chrono>
#include <cstdio>
#include <optional>
#include <unordered_map>

#include "DateUtils.hpp"

using namespace geode::prelude;

// Upload times we've already looked up this session, so reopening a level is instant.
static std::unordered_map<int, std::chrono::sys_seconds> s_cache;

class $modify(ExactDateLevelInfoLayer, LevelInfoLayer) {
    struct Fields {
        async::TaskHolder<web::WebResponse> m_listener;
        CCLabelBMFont* m_dateLabel = nullptr;

        // We temporarily swap the level's "x months" text for "x days" so the
        // game's own info popup shows days too. Put it back when we leave.
        Ref<GJGameLevel> m_level;
        std::string m_originalUploadText;
        std::string m_ourUploadText;

        ~Fields() {
            if (m_level && !m_ourUploadText.empty()
                && std::string(m_level->m_uploadDate) == m_ourUploadText) {
                m_level->m_uploadDate = m_originalUploadText;
            }
        }
    };

    bool init(GJGameLevel* level, bool challenge) {
        if (!LevelInfoLayer::init(level, challenge)) return false;

        int levelID = level->m_levelID.value();
        if (levelID <= 0) return true; // local / editor levels have no upload date

        this->addDateLabel();

        if (auto it = s_cache.find(levelID); it != s_cache.end()) {
            this->showUploadTime(it->second);
            return true;
        }

        auto req = web::WebRequest();
        req.userAgent("Geode exact-upload-date mod");
        req.timeout(std::chrono::seconds(10));

        m_fields->m_listener.spawn(
            "Fetch exact upload date",
            req.get(fmt::format("https://history.geometrydash.eu/api/v1/date/level/{}/", levelID)),
            [this, levelID](web::WebResponse res) {
                auto time = parseResponse(res);
                if (!time) {
                    log::warn("Couldn't get exact upload date for level {} (HTTP {})", levelID, res.code());
                    this->showFallback();
                    return;
                }
                s_cache[levelID] = *time;
                this->showUploadTime(*time);
            }
        );

        return true;
    }

    static std::optional<std::chrono::sys_seconds> parseResponse(web::WebResponse& res) {
        if (!res.ok()) return std::nullopt;
        auto json = res.json();
        if (!json) return std::nullopt;
        matjson::Value const& root = json.unwrap();

        // "approx" is GDHistory's best guess for this exact ID; low/high are the
        // nearest known levels either side of it.
        for (auto key : { "approx", "low", "high" }) {
            if (!root.contains(key)) continue;
            auto est = root[key]["estimation"].asString();
            if (!est) continue;
            if (auto t = date_utils::parseIso8601(est.unwrap())) return t;
        }
        return std::nullopt;
    }

    void addDateLabel() {
        auto winSize = CCDirector::get()->getWinSize();
        CCPoint pos = { winSize.width / 2.f, winSize.height - 62.f };

        // Sit just under the creator's name (node IDs come from Geode's Node IDs mod).
        if (auto name = this->querySelector("creator-info-menu > creator-name")) {
            auto world = name->getParent()->convertToWorldSpace(name->getPosition());
            auto local = this->convertToNodeSpace(world);
            pos = { local.x, local.y - name->getScaledContentSize().height / 2.f - 7.f };
        }

        auto label = CCLabelBMFont::create("Uploaded: loading...", "goldFont.fnt");
        label->setScale(0.42f);
        label->setPosition(pos);
        label->setOpacity(200);
        label->setID("exact-upload-date-label"_spr);
        this->addChild(label, 10);
        m_fields->m_dateLabel = label;
    }

    void showUploadTime(std::chrono::sys_seconds uploaded) {
        int days = date_utils::daysBetween(uploaded, std::chrono::system_clock::now());
        bool showDate = Mod::get()->getSettingValue<bool>("show-date");
        auto format = Mod::get()->getSettingValue<std::string>("date-format");

        std::string text = fmt::format("Uploaded {}", date_utils::daysAgoText(days));
        if (showDate) {
            text += fmt::format("  ({})", date_utils::formatDate(uploaded, format));
        }
        this->setLabelText(text);

        if (Mod::get()->getSettingValue<bool>("replace-vanilla-text")) {
            auto& f = m_fields;
            f->m_level = m_level;
            f->m_originalUploadText = std::string(m_level->m_uploadDate);
            // The game adds " ago" itself, so this reads e.g. "412 days ago".
            f->m_ourUploadText = days == 1 ? "1 day" : fmt::format("{} days", days);
            m_level->m_uploadDate = f->m_ourUploadText;
        }
    }

    void showFallback() {
        std::string vague = m_level->m_uploadDate;
        if (vague.empty()) {
            this->setLabelText("Uploaded: date unavailable");
        } else {
            this->setLabelText(fmt::format("Uploaded {} ago (exact date unavailable)", vague));
        }
    }

    void setLabelText(std::string const& text) {
        auto label = m_fields->m_dateLabel;
        if (!label) return;
        label->setString(text.c_str());
        // Keep long text from running off the sides of the screen.
        label->limitLabelWidth(300.f, 0.42f, 0.2f);
    }
};
