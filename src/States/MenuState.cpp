#include "../../include/States/MenuState.h"
#include "../../include/Core/ResourceManager.h"
#include "../../include/Core/StateManager.h"
#include "../../include/Utils/settings.h"
#include <filesystem>
#include <iostream>

void MenuState::onEnter() {
  std::cout << "=== MenuState: Entering ===" << std::endl;

  // 1. Загрузка ресурсов логотипа и шрифта
  logoTexture_ =
      ResourceManager::getInstance().getTexture(Assets::LOGO_TEXTURE);
  if (logoTexture_) {
    logoSprite_ = std::make_unique<sf::Sprite>(*logoTexture_);
    sf::FloatRect bounds = logoSprite_->getLocalBounds();
    logoSprite_->setOrigin({bounds.size.x / 2.f, bounds.size.y / 2.f});

    // Начальное положение анимации (падение логотипа)
    targetY_ = 220.f;
    logoSprite_->setPosition({VIRTUAL_CENTER_X, -200.f});
  }

  menuFont_ = ResourceManager::getInstance().getFont(Assets::FONT_PRIMARY);
  if (!menuFont_) {
    menuFont_ = ResourceManager::getInstance().getFont(Assets::FONT_FALLBACK_1);
  }

  // 2. Инициализация эффекта огня на всю виртуальную ширину и высоту
  fireEffect_ = std::make_unique<FireEffect>();

  // 3. Загрузка доступных карт
  loadAvailableMaps();

  // 4. Настройка текста выбора карты
  if (menuFont_) {
    mapNameText_ = std::make_unique<sf::Text>(*menuFont_);
    mapNameText_->setCharacterSize(28);
    mapNameText_->setFillColor(sf::Color::Yellow);
    cycleMap(0); // Обновляем строку с именем текущей карты
  }

  // 5. Размещение кнопок по центру виртуального экрана VIRTUAL_CENTER_X
  buttons_.clear();
  buttons_.push_back(std::make_unique<Button>(
      "Start Game", sf::Vector2f(VIRTUAL_CENTER_X, 680.f)));
  buttons_.push_back(std::make_unique<Button>(
      "Options", sf::Vector2f(VIRTUAL_CENTER_X, 780.f)));
  buttons_.push_back(
      std::make_unique<Button>("Exit", sf::Vector2f(VIRTUAL_CENTER_X, 880.f)));

  logoAnimationComplete_ = false;
  buttonsFadeAlpha_ = 0.f;
  totalTime_ = 0.f;

  std::cout << "=== MenuState: Ready ===" << std::endl;
}

void MenuState::onExit() {
  std::cout << "=== MenuState: Exiting ===" << std::endl;
  logoSprite_.reset();
  shineSprite_.reset();
  fireEffect_.reset();
  buttons_.clear();
  mapNameText_.reset();
  availableMaps_.clear();
  window_ = nullptr;
}

void MenuState::pause() {}
void MenuState::resume() {}

void MenuState::handleInput(const sf::Event &event) {
  if (!window_)
    return;

  // Виртуальный View 1920x1200 для расчетов позиций UI
  sf::View guiView(sf::FloatRect({0.f, 0.f}, {VIRTUAL_WIDTH, VIRTUAL_HEIGHT}));

  // 1. Движение мыши
  if (const auto *mouseMoved = event.getIf<sf::Event::MouseMoved>()) {
    sf::Vector2i pixelPos(mouseMoved->position.x, mouseMoved->position.y);
    mousePos_ = window_->mapPixelToCoords(pixelPos, guiView);
  }

  // Пропуск анимации логотипа
  if (!logoAnimationComplete_ || buttonsFadeAlpha_ < 255.f) {
    if (event.is<sf::Event::KeyPressed>() ||
        event.is<sf::Event::MouseButtonPressed>()) {
      skipAnimation();
      return;
    }
    return;
  }

  // 2. Навигация по картам клавишами Влево / Вправо
  if (const auto *keyPressed = event.getIf<sf::Event::KeyPressed>()) {
    if (keyPressed->code == sf::Keyboard::Key::Left) {
      cycleMap(-1);
      return;
    } else if (keyPressed->code == sf::Keyboard::Key::Right) {
      cycleMap(1);
      return;
    }
  }

  // 3. Клики мыши по кнопкам
  if (const auto *mousePressed = event.getIf<sf::Event::MouseButtonPressed>()) {
    if (mousePressed->button == sf::Mouse::Button::Left) {
      sf::Vector2i pixelPos(mousePressed->position.x, mousePressed->position.y);
      sf::Vector2f clickPos = window_->mapPixelToCoords(pixelPos, guiView);

      for (auto &button : buttons_) {
        if (button && button->isClicked(clickPos)) {
          handleButtonClick(button->getName());
          return;
        }
      }
    }
  }
}

void MenuState::update(float deltaTime) {
  totalTime_ += deltaTime;

  if (fireEffect_) {
    fireEffect_->update();
  }

  // Падение логотипа сверху
  if (logoSprite_ && !logoAnimationComplete_) {
    sf::Vector2f pos = logoSprite_->getPosition();
    if (pos.y < targetY_) {
      pos.y += 600.f * deltaTime;
      if (pos.y >= targetY_) {
        pos.y = targetY_;
        logoAnimationComplete_ = true;
      }
      logoSprite_->setPosition(pos);
    }
  }

  // Плавное проявление кнопок
  if (logoAnimationComplete_ && buttonsFadeAlpha_ < 255.f) {
    buttonsFadeAlpha_ = std::min(255.f, buttonsFadeAlpha_ + 400.f * deltaTime);
  }

  for (auto &button : buttons_) {
    if (button) {
      button->update(mousePos_, deltaTime);
    }
  }
}

void MenuState::render(sf::RenderWindow &window) {
  if (!window_) {
    window_ = &window;
  }

  // Включаем виртуальное разрешение 1920x1200
  sf::View guiView(sf::FloatRect({0.f, 0.f}, {VIRTUAL_WIDTH, VIRTUAL_HEIGHT}));
  window.setView(guiView);

  // 1. Задний фон с процедурным огнем
  if (fireEffect_) {
    fireEffect_->render(window);
  }

  // 2. Анимированный логотип
  if (logoSprite_) {
    window.draw(*logoSprite_);
  }

  // 3. Кнопки меню
  for (auto &button : buttons_) {
    if (button) {
      button->render(window);
    }
  }

  // 4. Текст выбора карт
  if (logoAnimationComplete_ && mapNameText_) {
    window.draw(*mapNameText_);
  }
}

void MenuState::skipAnimation() {
  logoAnimationComplete_ = true;
  buttonsFadeAlpha_ = 255.f;
  if (logoSprite_) {
    logoSprite_->setPosition({VIRTUAL_CENTER_X, targetY_});
  }
}

void MenuState::cycleMap(int direction) {
  if (availableMaps_.empty())
    return;

  selectedMapIndex_ = (selectedMapIndex_ + direction +
                       static_cast<int>(availableMaps_.size())) %
                      static_cast<int>(availableMaps_.size());

  if (mapNameText_) {
    std::string mapName = availableMaps_[selectedMapIndex_];
    mapNameText_->setString("< Map: " + mapName + " >");
    sf::FloatRect bounds = mapNameText_->getLocalBounds();
    mapNameText_->setOrigin({bounds.size.x / 2.f, bounds.size.y / 2.f});
    mapNameText_->setPosition({VIRTUAL_CENTER_X, 580.f});
  }
}

void MenuState::loadAvailableMaps() {
  availableMaps_.clear();
  std::string mapDir = "assets/maps/";

  if (std::filesystem::exists(mapDir)) {
    for (const auto &entry : std::filesystem::directory_iterator(mapDir)) {
      if (entry.is_regular_file() && entry.path().extension() == ".json") {
        availableMaps_.push_back(entry.path().filename().string());
      }
    }
  }

  if (availableMaps_.empty()) {
    availableMaps_.push_back("simple_step.json");
  }

  std::sort(availableMaps_.begin(), availableMaps_.end());
  selectedMapIndex_ = 0;
}

void MenuState::handleButtonClick(const std::string &buttonName) {
  if (buttonName == "Start Game") {
    std::string selectedMap =
        availableMaps_.empty()
            ? "assets/maps/simple_step.json"
            : "assets/maps/" + availableMaps_[selectedMapIndex_];
    std::cout << ">>> Starting Game with map: " << selectedMap << " <<<"
              << std::endl;
    if (stateManager_) {
      stateManager_->clearAndSwitchTo("Play");
    }
  } else if (buttonName == "Options") {
    std::cout << ">>> Opening Options <<<" << std::endl;
    if (stateManager_) {
      stateManager_->pushState("Options");
    }
  } else if (buttonName == "Exit") {
    std::cout << ">>> Exiting Game <<<" << std::endl;
    if (stateManager_) {
      stateManager_->exitGame();
    }
  }
}
