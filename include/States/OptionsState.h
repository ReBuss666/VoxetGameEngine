#pragma once
#include "../Core/GameSettings.h"
#include "../Core/GameState.h"
#include "../Core/ResourceManager.h"
#include "../Core/StateManager.h"
#include "../UI/Button.h"
#include "../Utils/settings.h"
#include <SFML/Graphics.hpp>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <vector>

class OptionsState : public GameState {
public:
  OptionsState() = default;

  void onEnter() override {
    std::cout << "=== OptionsState: Entering ===" << std::endl;

    font_ = ResourceManager::getInstance().getFont(Assets::FONT_PRIMARY);
    if (!font_) {
      font_ = ResourceManager::getInstance().getFont(Assets::FONT_FALLBACK_1);
    }

    if (font_) {
      titleText_ = std::make_unique<sf::Text>(*font_);
      titleText_->setString("OPTIONS");
      titleText_->setCharacterSize(48);
      titleText_->setFillColor(sf::Color::White);
      titleText_->setStyle(sf::Text::Bold);
      sf::FloatRect titleBounds = titleText_->getLocalBounds();
      titleText_->setOrigin(
          {titleBounds.size.x / 2.f, titleBounds.size.y / 2.f});
      titleText_->setPosition({VIRTUAL_CENTER_X, 80.f});
    }

    currentOption_ = 0;

    setupOptions();

    float buttonY = VIRTUAL_HEIGHT - 100.f;
    applyButton_ = std::make_unique<Button>(
        "Apply", sf::Vector2f(VIRTUAL_CENTER_X - 160.f, buttonY));
    backButton_ = std::make_unique<Button>(
        "Back", sf::Vector2f(VIRTUAL_CENTER_X + 160.f, buttonY));

    std::cout << "=== OptionsState: Ready ===" << std::endl;
  }

  void onExit() override {
    std::cout << "=== OptionsState: Exiting ===" << std::endl;
    titleText_.reset();
    applyButton_.reset();
    backButton_.reset();
    optionLabels_.clear();
    optionValues_.clear();
    window_ = nullptr;
  }

  void handleInput(const sf::Event &event) override {
    if (!window_)
      return;

    // Создаем виртуальный View 1920x1200 для расчетов UI
    sf::View guiView(
        sf::FloatRect({0.f, 0.f}, {VIRTUAL_WIDTH, VIRTUAL_HEIGHT}));

    if (const auto *keyPressed = event.getIf<sf::Event::KeyPressed>()) {
      handleKeyPress(keyPressed->code);
    }

    // Обработка движения мыши с учетом guiView
    if (const auto *mouseMoved = event.getIf<sf::Event::MouseMoved>()) {
      sf::Vector2i pixelPos(mouseMoved->position.x, mouseMoved->position.y);
      mousePos_ = window_->mapPixelToCoords(pixelPos, guiView);
    }

    // Обработка кликов мыши с учетом guiView
    if (const auto *mousePressed =
            event.getIf<sf::Event::MouseButtonPressed>()) {
      if (mousePressed->button == sf::Mouse::Button::Left) {
        sf::Vector2i pixelPos(mousePressed->position.x,
                              mousePressed->position.y);
        sf::Vector2f clickPos = window_->mapPixelToCoords(pixelPos, guiView);
        handleMouseClick(clickPos);
      }
    }
  }

  void update(float deltaTime) override {
    // Update buttons
    if (applyButton_)
      applyButton_->update(mousePos_, deltaTime);
    if (backButton_)
      backButton_->update(mousePos_, deltaTime);
  }

  void render(sf::RenderWindow &window) override {
    if (!window_) {
      window_ = &window;
    }

    // Включаем виртуальное разрешение 1920x1200
    sf::View guiView(
        sf::FloatRect({0.f, 0.f}, {VIRTUAL_WIDTH, VIRTUAL_HEIGHT}));
    window.setView(guiView);

    sf::RectangleShape background;
    background.setSize({VIRTUAL_WIDTH, VIRTUAL_HEIGHT});
    background.setFillColor(sf::Color(20, 20, 30));
    window.draw(background);

    if (titleText_) {
      window.draw(*titleText_);
    }

    drawOptionsPanel(window);

    if (applyButton_)
      applyButton_->render(window);
    if (backButton_)
      backButton_->render(window);

    drawInstructions(window);
  }

private:
  sf::Font *font_ = nullptr;
  std::unique_ptr<sf::Text> titleText_;
  std::unique_ptr<Button> applyButton_;
  std::unique_ptr<Button> backButton_;
  sf::RenderWindow *window_ = nullptr;

  sf::Vector2f mousePos_{0.f, 0.f};
  int currentOption_ = 0;

  struct Option {
    std::string name;
    std::string value;
    enum Type { CHOICE, TOGGLE, SLIDER } type;
    float sliderValue = 0.f;
    int choiceIndex = 0;
    std::vector<std::string> choices;
  };
  std::vector<Option> options_;
  std::vector<std::unique_ptr<sf::Text>> optionLabels_;
  std::vector<std::unique_ptr<sf::Text>> optionValues_;

  void setupOptions() {
    options_.clear();
    optionLabels_.clear();
    optionValues_.clear();

    auto &settings = GameSettings::getInstance();

    Option resOption;
    resOption.name = "Resolution";
    resOption.type = Option::CHOICE;
    for (const auto &res : settings.getAvailableResolutions()) {
      resOption.choices.push_back(res.toString());
    }
    resOption.choiceIndex = settings.getCurrentResolutionIndex();
    resOption.value = resOption.choices[resOption.choiceIndex];
    options_.push_back(resOption);

    Option fsOption;
    fsOption.name = "Fullscreen";
    fsOption.type = Option::TOGGLE;
    fsOption.value = settings.isFullscreen() ? "ON" : "OFF";
    options_.push_back(fsOption);

    Option vsOption;
    vsOption.name = "VSync";
    vsOption.type = Option::TOGGLE;
    vsOption.value = settings.isVSyncEnabled() ? "ON" : "OFF";
    options_.push_back(vsOption);

    Option fpsOption;
    fpsOption.name = "FPS Limit";
    fpsOption.type = Option::CHOICE;
    fpsOption.choices = {"30", "60", "90", "120", "144", "240"};
    int fps = settings.getFPSLimit();
    for (size_t i = 0; i < fpsOption.choices.size(); ++i) {
      if (std::stoi(fpsOption.choices[i]) == fps) {
        fpsOption.choiceIndex = static_cast<int>(i);
        break;
      }
    }
    fpsOption.value = std::to_string(fps);
    options_.push_back(fpsOption);

    Option sensOption;
    sensOption.name = "Mouse Sensitivity";
    sensOption.type = Option::SLIDER;
    sensOption.sliderValue = settings.getMouseSensitivity();
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1) << sensOption.sliderValue;
    sensOption.value = ss.str();
    options_.push_back(sensOption);

    Option fovOption;
    fovOption.name = "Field of View";
    fovOption.type = Option::SLIDER;
    fovOption.sliderValue = settings.getFOV();
    fovOption.value =
        std::to_string(static_cast<int>(fovOption.sliderValue)) + "°";
    options_.push_back(fovOption);

    Option volOption;
    volOption.name = "Master Volume";
    volOption.type = Option::SLIDER;
    volOption.sliderValue = settings.getMasterVolume() * 100.f;
    volOption.value =
        std::to_string(static_cast<int>(volOption.sliderValue)) + "%";
    options_.push_back(volOption);

    Option musicOption;
    musicOption.name = "Music Volume";
    musicOption.type = Option::SLIDER;
    musicOption.sliderValue = settings.getMusicVolume() * 100.f;
    musicOption.value =
        std::to_string(static_cast<int>(musicOption.sliderValue)) + "%";
    options_.push_back(musicOption);

    if (font_) {
      float startY = 160.f;
      float spacing = 50.f;

      for (size_t i = 0; i < options_.size(); ++i) {
        auto label = std::make_unique<sf::Text>(*font_);
        label->setString(options_[i].name);
        label->setCharacterSize(24);
        label->setFillColor(sf::Color::White);
        label->setPosition({VIRTUAL_CENTER_X - 300.f, startY + i * spacing});
        optionLabels_.push_back(std::move(label));

        auto value = std::make_unique<sf::Text>(*font_);
        value->setString(options_[i].value);
        value->setCharacterSize(24);
        value->setFillColor(sf::Color::Cyan);
        value->setPosition({VIRTUAL_CENTER_X + 100.f, startY + i * spacing});
        optionValues_.push_back(std::move(value));
      }
    }
  }

  void handleKeyPress(sf::Keyboard::Key key) {
    if (key == sf::Keyboard::Key::Escape) {
      if (stateManager_)
        stateManager_->popState();
      return;
    }

    if (key == sf::Keyboard::Key::Up) {
      currentOption_ =
          (currentOption_ - 1 + static_cast<int>(options_.size())) %
          options_.size();
    } else if (key == sf::Keyboard::Key::Down) {
      currentOption_ = (currentOption_ + 1) % options_.size();
    } else if (key == sf::Keyboard::Key::Left) {
      adjustOption(-1);
    } else if (key == sf::Keyboard::Key::Right) {
      adjustOption(1);
    } else if (key == sf::Keyboard::Key::Enter) {
      if (options_[currentOption_].type == Option::TOGGLE) {
        adjustOption(1); // Toggle
      }
    }
  }

  void adjustOption(int direction) {
    Option &opt = options_[currentOption_];
    auto &settings = GameSettings::getInstance();

    switch (opt.type) {
    case Option::CHOICE:
      opt.choiceIndex =
          (opt.choiceIndex + direction + static_cast<int>(opt.choices.size())) %
          opt.choices.size();
      opt.value = opt.choices[opt.choiceIndex];

      if (opt.name == "Resolution") {
        settings.setResolutionByIndex(opt.choiceIndex);
      } else if (opt.name == "FPS Limit") {
        settings.setFPSLimit(std::stoi(opt.value));
      }
      break;

    case Option::TOGGLE:
      opt.value = (opt.value == "ON") ? "OFF" : "ON";

      if (opt.name == "Fullscreen") {
        settings.setFullscreen(opt.value == "ON");
      } else if (opt.name == "VSync") {
        settings.setVSync(opt.value == "ON");
      }
      break;

    case Option::SLIDER:
      if (opt.name == "Mouse Sensitivity") {
        opt.sliderValue =
            std::clamp(opt.sliderValue + direction * 0.1f, 0.1f, 3.0f);
        settings.setMouseSensitivity(opt.sliderValue);
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1) << opt.sliderValue;
        opt.value = ss.str();
      } else if (opt.name == "Field of View") {
        opt.sliderValue =
            std::clamp(opt.sliderValue + direction * 5.f, 45.f, 120.f);
        settings.setFOV(opt.sliderValue);
        opt.value = std::to_string(static_cast<int>(opt.sliderValue)) + "°";
      } else if (opt.name == "Master Volume") {
        opt.sliderValue =
            std::clamp(opt.sliderValue + direction * 5.f, 0.f, 100.f);
        settings.setMasterVolume(opt.sliderValue / 100.f);
        opt.value = std::to_string(static_cast<int>(opt.sliderValue)) + "%";
      } else if (opt.name == "Music Volume") {
        opt.sliderValue =
            std::clamp(opt.sliderValue + direction * 5.f, 0.f, 100.f);
        settings.setMusicVolume(opt.sliderValue / 100.f);
        opt.value = std::to_string(static_cast<int>(opt.sliderValue)) + "%";
      }
      break;
    }

    if (currentOption_ < static_cast<int>(optionValues_.size())) {
      optionValues_[currentOption_]->setString(opt.value);
    }
  }

  void handleMouseClick(sf::Vector2f pos) {
    if (applyButton_ && applyButton_->isClicked(pos)) {
      applySettings();
      return;
    }

    if (backButton_ && backButton_->isClicked(pos)) {
      if (stateManager_)
        stateManager_->popState();
      return;
    }

    float startY = 160.f;
    float spacing = 50.f;
    float optionHeight = 40.f;

    for (size_t i = 0; i < options_.size(); ++i) {
      float optY = startY + i * spacing;
      if (pos.y >= optY && pos.y <= optY + optionHeight) {
        currentOption_ = static_cast<int>(i);

        if (pos.x > VIRTUAL_CENTER_X) {
          if (pos.x < VIRTUAL_CENTER_X + 150.f) {
            adjustOption(-1);
          } else {
            adjustOption(1);
          }
        }
        break;
      }
    }
  }

  void applySettings() {
    std::cout << "[OptionsState] Applying settings..." << std::endl;

    auto &settings = GameSettings::getInstance();
    settings.saveSettings();

    if (stateManager_) {
      std::cout << "[OptionsState] Settings saved. Restart may be needed for "
                   "some changes."
                << std::endl;
    }
  }

  void drawOptionsPanel(sf::RenderWindow &window) {
    sf::RectangleShape panel;
    float panelWidth = 700.f;
    float panelHeight = 60.f + options_.size() * 50.f;
    panel.setSize({panelWidth, panelHeight});
    panel.setPosition({VIRTUAL_CENTER_X - panelWidth / 2.f, 130.f});
    panel.setFillColor(sf::Color(40, 40, 50, 200));
    panel.setOutlineColor(sf::Color(100, 100, 120));
    panel.setOutlineThickness(2.f);
    window.draw(panel);

    for (size_t i = 0; i < options_.size(); ++i) {
      if (static_cast<int>(i) == currentOption_) {
        sf::RectangleShape highlight;
        highlight.setSize({panelWidth - 20.f, 40.f});
        highlight.setPosition(
            {VIRTUAL_CENTER_X - panelWidth / 2.f + 10.f, 155.f + i * 50.f});
        highlight.setFillColor(sf::Color(60, 60, 80, 150));
        window.draw(highlight);
      }

      if (i < optionLabels_.size())
        window.draw(*optionLabels_[i]);
      if (i < optionValues_.size())
        window.draw(*optionValues_[i]);

      if (font_ && (options_[i].type == Option::CHOICE ||
                    options_[i].type == Option::SLIDER)) {
        sf::Text leftArrow(*font_);
        leftArrow.setString("<");
        leftArrow.setCharacterSize(24);
        leftArrow.setFillColor(sf::Color(150, 150, 150));
        leftArrow.setPosition({VIRTUAL_CENTER_X + 60.f, 160.f + i * 50.f});
        window.draw(leftArrow);

        sf::Text rightArrow(*font_);
        rightArrow.setString(">");
        rightArrow.setCharacterSize(24);
        rightArrow.setFillColor(sf::Color(150, 150, 150));
        rightArrow.setPosition({VIRTUAL_CENTER_X + 250.f, 160.f + i * 50.f});
        window.draw(rightArrow);
      }

      if (options_[i].type == Option::SLIDER) {
        drawSlider(window, i);
      }
    }
  }

  void drawSlider(sf::RenderWindow &window, size_t optionIndex) {
    float sliderX = VIRTUAL_CENTER_X + 80.f;
    float sliderY = 170.f + optionIndex * 50.f;
    float sliderWidth = 160.f;
    float sliderHeight = 8.f;

    sf::RectangleShape bar;
    bar.setSize({sliderWidth, sliderHeight});
    bar.setPosition({sliderX, sliderY});
    bar.setFillColor(sf::Color(60, 60, 70));
    window.draw(bar);

    float fillRatio = 0.f;
    const Option &opt = options_[optionIndex];
    if (opt.name == "Mouse Sensitivity") {
      fillRatio = (opt.sliderValue - 0.1f) / 2.9f;
    } else if (opt.name == "Field of View") {
      fillRatio = (opt.sliderValue - 45.f) / 75.f;
    } else {
      fillRatio = opt.sliderValue / 100.f;
    }

    sf::RectangleShape fill;
    fill.setSize({sliderWidth * fillRatio, sliderHeight});
    fill.setPosition({sliderX, sliderY});
    fill.setFillColor(sf::Color(100, 180, 255));
    window.draw(fill);

    sf::CircleShape knob(8.f);
    knob.setOrigin({8.f, 8.f});
    knob.setPosition(
        {sliderX + sliderWidth * fillRatio, sliderY + sliderHeight / 2.f});
    knob.setFillColor(sf::Color::White);
    window.draw(knob);
  }

  void drawInstructions(sf::RenderWindow &window) {
    if (!font_)
      return;

    sf::Text instructions(*font_);
    instructions.setString(
        "UP/DOWN: Select   LEFT/RIGHT: Adjust   ENTER: Toggle   ESC: Back");
    instructions.setCharacterSize(16);
    instructions.setFillColor(sf::Color(150, 150, 150));
    sf::FloatRect bounds = instructions.getLocalBounds();
    instructions.setOrigin({bounds.size.x / 2.f, bounds.size.y / 2.f});
    instructions.setPosition({VIRTUAL_CENTER_X, VIRTUAL_HEIGHT - 30.f});
    window.draw(instructions);
  }
};
