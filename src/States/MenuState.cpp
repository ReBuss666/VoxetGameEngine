#include "../../include/States/MenuState.h"
#include <cmath>
#include <iostream>

void MenuState::onEnter() {
  std::cout << "=== MenuState: Loading resources ===" << std::endl;

  std::cout << "[MenuState] Loading logo from: assets/logo.png" << std::endl;

  logoTexture_ = ResourceManager::getInstance().getTexture("assets/VOXET.png");

  if (!logoTexture_) {
    std::cerr << "[MenuState] FAILED to load logo texture!" << std::endl;
    std::cerr << "[MenuState] Make sure to run from 'build' folder!"
              << std::endl;
  } else {
    std::cout << "[MenuState] Logo loaded: " << logoTexture_->getSize().x << "x"
              << logoTexture_->getSize().y << std::endl;
    logoTexture_->setSmooth(false);

    logoSprite_ = std::make_unique<sf::Sprite>(*logoTexture_);
    sf::FloatRect bounds = logoSprite_->getLocalBounds();
    logoSprite_->setOrigin({bounds.size.x / 2.f, bounds.size.y / 2.f});
    logoSprite_->setPosition(
        {WINDOW_CENTER_X, WINDOW_HEIGHT + bounds.size.y / 2.f + 10.f});

    // Желаемая ширина логотипа в игре (например, 500 пикселей)
    float desiredWidth = 480.f;
    float scale = desiredWidth / bounds.size.x;
    logoSprite_->setScale({scale, scale});

    shineSprite_ = std::make_unique<sf::Sprite>(*logoTexture_);
    shineSprite_->setOrigin(logoSprite_->getOrigin());
    shineSprite_->setScale(
        logoSprite_->getScale()); // Автоматически применит правильный scale
    shineSprite_->setColor(sf::Color(255, 255, 255, 170));
  }

  fireEffect_ =
      std::make_unique<FireEffect>(window_WIDTH, window_HEIGHT, PixelSize);
  fireEffect_->triggerFlash();

  buttons_.reserve(3);
  buttons_.push_back(std::make_unique<Button>(
      "Start Game", sf::Vector2f(WINDOW_CENTER_X, MENU_BUTTON_START_Y)));
  buttons_.push_back(std::make_unique<Button>(
      "Options", sf::Vector2f(WINDOW_CENTER_X,
                              MENU_BUTTON_START_Y + MENU_BUTTON_SPACING)));
  buttons_.push_back(std::make_unique<Button>(
      "Exit", sf::Vector2f(WINDOW_CENTER_X,
                           MENU_BUTTON_START_Y + MENU_BUTTON_SPACING * 2)));

  buttonsFadeAlpha_ = 0.f;
  for (auto &button : buttons_) {
    button->setAlpha(0.f);
  }

  // Initialize available maps from assets/maps/ directory
  loadAvailableMaps();
  selectedMapIndex_ = 0;

  // Setup map name text
  menuFont_ = ResourceManager::getInstance().getFont(Assets::FONT_PRIMARY);
  if (menuFont_) {
    mapNameText_ = std::make_unique<sf::Text>(
        *menuFont_, "< " + availableMaps_[selectedMapIndex_] + " >", 24);
    mapNameText_->setFillColor(sf::Color(255, 200, 100));
    mapNameText_->setOutlineColor(sf::Color::Black);
    mapNameText_->setOutlineThickness(2.f);

    sf::FloatRect textBounds = mapNameText_->getLocalBounds();
    mapNameText_->setOrigin({textBounds.size.x / 2.f, textBounds.size.y / 2.f});
    mapNameText_->setPosition({WINDOW_CENTER_X, MENU_BUTTON_START_Y - 60.f});
  }

  totalTime_ = 0.f;
  shinePos_ = -1.f;
  targetY_ = WINDOW_HEIGHT / 2.5f - LOGO_OFFSET_Y;
  logoAnimationComplete_ = false;

  std::cout << "=== MenuState: Loaded ===" << std::endl;
}

void MenuState::onExit() {
  std::cout << "=== MenuState: Releasing resources ===" << std::endl;
  buttons_.clear();
  fireEffect_.reset();
  shineSprite_.reset();
  logoSprite_.reset();
  logoTexture_ = nullptr;
  std::cout << "=== MenuState: Resources released ===" << std::endl;
}

void MenuState::pause() {
  std::cout << "=== MenuState: Paused ===" << std::endl;
  if (fireEffect_) {
    fireEffect_->disableFuel();
  }
}

void MenuState::resume() {
  std::cout << "=== MenuState: Resumed ===" << std::endl;
  if (fireEffect_) {
    fireEffect_->enableFuel();
  }
}

void MenuState::handleInput(const sf::Event &event) {
  if (!logoAnimationComplete_ || buttonsFadeAlpha_ < 255.f) {
    if (event.is<sf::Event::KeyPressed>() ||
        event.is<sf::Event::MouseButtonPressed>()) {
      skipAnimation();
      return;
    }

    if (const auto *mouseMoved = event.getIf<sf::Event::MouseMoved>()) {
      mousePos_ = sf::Vector2f(static_cast<float>(mouseMoved->position.x),
                               static_cast<float>(mouseMoved->position.y));
    }
    return;
  }

  // Handle map selection with arrow keys
  if (const auto *keyPressed = event.getIf<sf::Event::KeyPressed>()) {
    if (keyPressed->code == sf::Keyboard::Key::Left) {
      cycleMap(-1);
      return;
    } else if (keyPressed->code == sf::Keyboard::Key::Right) {
      cycleMap(1);
      return;
    }
  }

  if (const auto *mousePressed = event.getIf<sf::Event::MouseButtonPressed>()) {
    sf::Vector2f mousePos(static_cast<float>(mousePressed->position.x),
                          static_cast<float>(mousePressed->position.y));

    for (auto &button : buttons_) {
      if (button && button->isClicked(mousePos)) {
        handleButtonClick(button->getName());
        return;
      }
    }
  }

  if (const auto *mouseMoved = event.getIf<sf::Event::MouseMoved>()) {
    mousePos_ = sf::Vector2f(static_cast<float>(mouseMoved->position.x),
                             static_cast<float>(mouseMoved->position.y));
  }
}

void MenuState::update(float deltaTime) {
  totalTime_ += deltaTime;

  if (fireEffect_) {
    fireEffect_->update();
  }

  if (logoSprite_) {
    sf::Vector2f currentPos = logoSprite_->getPosition();
    if (currentPos.y > targetY_) {
      logoSprite_->move({0.f, -LOGO_SPEED * deltaTime});
    } else {
      if (!logoAnimationComplete_) {
        logoAnimationComplete_ = true;
        std::cout << "=== Logo animation complete ===" << std::endl;
      }
      float offset = std::sin(totalTime_ * 2.0f) * 2.0f;
      logoSprite_->setPosition({WINDOW_CENTER_X, targetY_ + offset});
    }

    if (shineSprite_) {
      shineSprite_->setPosition(logoSprite_->getPosition());
    }
  }

  shinePos_ += deltaTime * 1.5f;
  if (shinePos_ > 2.f)
    shinePos_ = -1.f;

  float shineFactor = std::max(0.0f, 1.0f - std::abs(shinePos_ - 0.5f) * 4.0f);
  if (shineSprite_) {
    shineSprite_->setColor(
        sf::Color(255, 255, 255, static_cast<std::uint8_t>(shineFactor * 150)));
  }

  if (logoAnimationComplete_ && buttonsFadeAlpha_ < 255.f) {
    buttonsFadeAlpha_ += 300.f * deltaTime;
    if (buttonsFadeAlpha_ > 255.f) {
      buttonsFadeAlpha_ = 255.f;
    }

    for (auto &button : buttons_) {
      button->setAlpha(buttonsFadeAlpha_);
    }
  }

  if (logoAnimationComplete_) {
    for (auto &button : buttons_) {
      button->update(mousePos_);
    }

    // Update map name text alpha
    if (mapNameText_) {
      sf::Color textColor = mapNameText_->getFillColor();
      textColor.a = static_cast<uint8_t>(buttonsFadeAlpha_);
      mapNameText_->setFillColor(textColor);
    }
  }
}

void MenuState::render(sf::RenderWindow &window) {
  if (logoSprite_) {
    window.draw(*logoSprite_);
  }

  if (fireEffect_) {
    fireEffect_->render(window);
  }

  if (shineSprite_) {
    window.draw(*shineSprite_, sf::BlendAdd);
  }

  for (auto &button : buttons_) {
    button->render(window);
  }

  // Render map selection text
  if (logoAnimationComplete_ && menuFont_ && mapNameText_) {
    window.draw(*mapNameText_);
  }
}

void MenuState::skipAnimation() {
  if (logoAnimationComplete_ && buttonsFadeAlpha_ >= 255.f) {
    return;
  }

  std::cout << "=== Skipping animation ===" << std::endl;

  if (logoSprite_) {
    logoSprite_->setPosition({WINDOW_CENTER_X, targetY_});
  }

  if (fireEffect_) {
    fireEffect_->triggerFlash();
  }

  logoAnimationComplete_ = true;
  buttonsFadeAlpha_ = 255.f;

  for (auto &button : buttons_) {
    button->setAlpha(255.f);
  }
}

void MenuState::handleButtonClick(const std::string &buttonName) {
  if (buttonName == "Start Game") {
    std::cout << ">>> Start Game clicked <<<" << std::endl;
    std::cout << ">>> Selected map: " << availableMaps_[selectedMapIndex_]
              << " <<<" << std::endl;

    // Convert map name back to filename (lowercase, spaces to underscores)
    std::string mapFileName = availableMaps_[selectedMapIndex_];
    for (char &c : mapFileName) {
      if (c == ' ') {
        c = '_';
      } else {
        c = static_cast<char>(std::tolower(c));
      }
    }

    std::string mapPath = "assets/maps/" + mapFileName + ".json";
    std::cout << ">>> Map file: " << mapPath << " <<<" << std::endl;

    // Update StateManager with selected map file
    if (stateManager_) {
      stateManager_->setMapPath(mapPath);
    }

    stateManager_->pushState("Play");
  } else if (buttonName == "Options") {
    std::cout << ">>> Options clicked <<<" << std::endl;
    stateManager_->pushState("Options");
  } else if (buttonName == "Exit") {
    std::cout << ">>> Exit clicked <<<" << std::endl;
    stateManager_->exitGame();
  }
}

void MenuState::cycleMap(int direction) {
  selectedMapIndex_ += direction;

  if (selectedMapIndex_ < 0) {
    selectedMapIndex_ = static_cast<int>(availableMaps_.size()) - 1;
  } else if (selectedMapIndex_ >= static_cast<int>(availableMaps_.size())) {
    selectedMapIndex_ = 0;
  }

  if (mapNameText_) {
    mapNameText_->setString("< " + availableMaps_[selectedMapIndex_] + " >");
    sf::FloatRect textBounds = mapNameText_->getLocalBounds();
    mapNameText_->setOrigin({textBounds.size.x / 2.f, textBounds.size.y / 2.f});
  }

  std::cout << "[MenuState] Selected map: " << availableMaps_[selectedMapIndex_]
            << std::endl;
}

void MenuState::loadAvailableMaps() {
  availableMaps_.clear();

  const std::string mapsPath = "assets/maps";

  try {
    if (!std::filesystem::exists(mapsPath)) {
      std::cerr << "[MenuState] Maps directory not found: " << mapsPath
                << std::endl;
      availableMaps_.push_back("No Maps Found");
      return;
    }

    // Scan directory for .json files
    for (const auto &entry : std::filesystem::directory_iterator(mapsPath)) {
      if (entry.is_regular_file() && entry.path().extension() == ".json") {
        // Get filename without extension
        std::string mapName = entry.path().stem().string();

        // Convert underscores to spaces and capitalize
        for (size_t i = 0; i < mapName.length(); ++i) {
          if (mapName[i] == '_') {
            mapName[i] = ' ';
          }
          if (i == 0 || mapName[i - 1] == ' ') {
            mapName[i] = static_cast<char>(std::toupper(mapName[i]));
          }
        }

        availableMaps_.push_back(mapName);
        std::cout << "[MenuState] Found map: " << mapName << " ("
                  << entry.path().filename().string() << ")" << std::endl;
      }
    }

    // Sort maps alphabetically
    std::sort(availableMaps_.begin(), availableMaps_.end());

    if (availableMaps_.empty()) {
      std::cerr << "[MenuState] No .json maps found in " << mapsPath
                << std::endl;
      availableMaps_.push_back("No Maps Available");
    } else {
      std::cout << "[MenuState] Loaded " << availableMaps_.size() << " map(s)"
                << std::endl;
    }
  } catch (const std::filesystem::filesystem_error &e) {
    std::cerr << "[MenuState] Filesystem error: " << e.what() << std::endl;
    availableMaps_.push_back("Error Loading Maps");
  }
}
