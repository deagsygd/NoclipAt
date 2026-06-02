#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>

using namespace geode::prelude;

// ============================================================================
// NoclipAtPopup — The popup that lets users set the noclip percentage
// ============================================================================

class NoclipAtPopup : public geode::Popup {
protected:
  TextInput *m_input = nullptr;

  bool init() {
    if (!Popup::init(260.f, 200.f))
      return false;

    this->setTitle("NoclipAt");

    // --- Current value label ---
    int currentPercent =
        Mod::get()->getSettingValue<int64_t>("noclip-percentage");

    auto statusStr = currentPercent > 0
                         ? fmt::format("Currently set to: {}%", currentPercent)
                         : std::string("Currently: Disabled");

    auto statusLabel = CCLabelBMFont::create(statusStr.c_str(), "bigFont.fnt");
    statusLabel->setScale(0.4f);
    m_mainLayer->addChildAtPosition(statusLabel, Anchor::Center, ccp(0, 50));

    // --- Help text ---
    auto helpLabel =
        CCLabelBMFont::create("Enter percentage (1-100):", "chatFont.fnt");
    helpLabel->setScale(0.7f);
    m_mainLayer->addChildAtPosition(helpLabel, Anchor::Center, ccp(0, 22));

    // --- Text input ---
    m_input = TextInput::create(120.f, "0");
    m_input->setCommonFilter(CommonFilter::Uint);
    m_input->setMaxCharCount(3);
    if (currentPercent > 0) {
      m_input->setString(std::to_string(currentPercent));
    }
    m_mainLayer->addChildAtPosition(m_input, Anchor::Center, ccp(0, -10));

    // --- Apply button ---
    auto applySpr =
        ButtonSprite::create("Apply", "goldFont.fnt", "GJ_button_01.png", 0.8f);
    auto applyBtn = CCMenuItemSpriteExtra::create(
        applySpr, this, menu_selector(NoclipAtPopup::onApply));
    m_buttonMenu->addChildAtPosition(applyBtn, Anchor::Center, ccp(-55, -55));

    // --- Disable button ---
    auto disableSpr = ButtonSprite::create("Disable", "goldFont.fnt",
                                           "GJ_button_06.png", 0.8f);
    auto disableBtn = CCMenuItemSpriteExtra::create(
        disableSpr, this, menu_selector(NoclipAtPopup::onDisable));
    m_buttonMenu->addChildAtPosition(disableBtn, Anchor::Center, ccp(55, -55));

    return true;
  }

  void onApply(CCObject *) {
    std::string text = m_input->getString();
    if (text.empty()) {
      FLAlertLayer::create("NoclipAt", "Please enter a percentage!", "OK")
          ->show();
      return;
    }

    auto result = geode::utils::numFromString<int>(text);
    if (result.isErr()) {
      FLAlertLayer::create("NoclipAt", "Invalid number!", "OK")->show();
      return;
    }
    int value = result.unwrap();

    if (value < 1 || value > 100) {
      FLAlertLayer::create(
          "NoclipAt", "Percentage must be between <cl>1</c> and <cl>100</c>!",
          "OK")
          ->show();
      return;
    }

    Mod::get()->setSettingValue<int64_t>("noclip-percentage", value);

    FLAlertLayer::create(
        "NoclipAt",
        fmt::format("Noclip will activate at <cg>{}%</c>!", value).c_str(),
        "OK")
        ->show();

    this->onClose(nullptr);
  }

  void onDisable(CCObject *) {
    Mod::get()->setSettingValue<int64_t>("noclip-percentage", 0);

    FLAlertLayer::create("NoclipAt", "Noclip has been <cr>disabled</c>.", "OK")
        ->show();

    this->onClose(nullptr);
  }

public:
  static NoclipAtPopup *create() {
    auto ret = new NoclipAtPopup();
    if (ret->init()) {
      ret->autorelease();
      return ret;
    }
    delete ret;
    return nullptr;
  }
};

// ============================================================================
// PauseLayer Hook — Add the NoclipAt button to the pause menu
// ============================================================================

#include <Geode/modify/PauseLayer.hpp>

class $modify(NoclipAtPauseLayer, PauseLayer) {
  void customSetup() {
    PauseLayer::customSetup();

    // Create a labeled button
    auto btnSpr = ButtonSprite::create("NoclipAt", "goldFont.fnt",
                                       "GJ_button_04.png", 0.8f);
    btnSpr->setScale(0.65f);

    auto btn = CCMenuItemSpriteExtra::create(
        btnSpr, this, menu_selector(NoclipAtPauseLayer::onNoclipAt));

    // Position in the top-right area of the screen
    auto winSize = CCDirector::sharedDirector()->getWinSize();

    // Create a new menu for our button so positioning is straightforward
    auto menu = CCMenu::create();
    menu->setPosition(60.f, 25.f);
    menu->addChild(btn);
    btn->setPosition(ccp(0, 0));

    this->addChild(menu, 10);
  }

  void onNoclipAt(CCObject *) { NoclipAtPopup::create()->show(); }
};

// ============================================================================
// PlayLayer Hook — Core noclip logic
// ============================================================================

#include <Geode/modify/PlayLayer.hpp>

class $modify(NoclipAtPlayLayer, PlayLayer) {
  struct Fields {
    bool m_noclipActive = false;
    float m_noclipPercent = 0.f;
  };

  void resetLevel() {
    PlayLayer::resetLevel();

    // Read the setting and cache it
    m_fields->m_noclipPercent = static_cast<float>(
        Mod::get()->getSettingValue<int64_t>("noclip-percentage"));
    m_fields->m_noclipActive = false;

    // Ensure noclip is off at the start of the attempt
    this->toggleIgnoreDamage(false);
  }

  void postUpdate(float dt) {
    PlayLayer::postUpdate(dt);

    // Only run if a threshold is set and noclip hasn't activated yet
    if (m_fields->m_noclipPercent > 0.f && !m_fields->m_noclipActive) {
      float currentPercent = this->getCurrentPercent();

      if (currentPercent >= m_fields->m_noclipPercent) {
        m_fields->m_noclipActive = true;
        this->toggleIgnoreDamage(true);

        log::debug("NoclipAt: Noclip activated at {:.1f}% (threshold: {}%)",
                   currentPercent, m_fields->m_noclipPercent);
      }
    }
  }

  void destroyPlayer(PlayerObject *player, GameObject *object) {
    // If noclip is active, skip the death entirely
    if (m_fields->m_noclipActive) {
      log::debug("NoclipAt: Death blocked by noclip");
      return;
    }

    // Otherwise, die normally
    PlayLayer::destroyPlayer(player, object);
  }
};