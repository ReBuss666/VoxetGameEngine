#include "../../include/States/MenuState.h"
#include "../../include/Core/ResourceManager.h"
#include "../../include/Core/StateManager.h"
#include "../../include/Utils/settings.h"
#include <filesystem>
#include <iostream>

static const std::string SHINE_SHADER_SRC = R"(
    uniform sampler2D texture;
    uniform vec2 texSize;
    uniform float shinePos;

    void main() {
        vec2 uv = gl_TexCoord[0].xy;
        vec4 pixel = texture2D(texture, uv);

        if (pixel.a < 0.1) {
            discard;
        }

        vec2 texel = floor(uv * texSize);

        float diagTexel = texel.x + texel.y;
        float maxDiag = texSize.x + texSize.y;
        float normDiag = diagTexel / maxDiag;

        float width = 0.05;
        float dist = abs(normDiag - shinePos);

        if (dist < width) {
            float rawIntensity = 1.0 - (dist / width);
            
            float steps = 3.0;
            float quantized = ceil(rawIntensity * steps) / steps;

            pixel.rgb += vec3(quantized * 0.55);
        }

        gl_FragColor = pixel * gl_Color;
    }
)";

void MenuState::onEnter() {
  std::cout << "=== MenuState: Entering ===" << std::endl;

  // 1. Загрузка ресурсов логотипа
  logoTexture_ =
      ResourceManager::getInstance().getTexture(Assets::LOGO_TEXTURE);
  if (logoTexture_) {
    logoTexture_->setSmooth(false);
    logoSprite_ = std::make_unique<sf::Sprite>(*logoTexture_);
    sf::FloatRect bounds = logoSprite_->getLocalBounds();
    logoSprite_->setOrigin({bounds.size.x / 2.f, bounds.size.y / 2.f});

    // 1920x1200)
    float desiredWidth = 600.f;
    float scale = desiredWidth / bounds.size.x;
    logoSprite_->setScale({scale, scale});

    targetY_ = 400.f;
    logoSprite_->setPosition({VIRTUAL_CENTER_X, VIRTUAL_HEIGHT + 250.f});
  } else {
    std::cerr << "[ERROR] Could not load logo texture: " << Assets::LOGO_TEXTURE
              << std::endl;
  }

  // 2. Инициализация и настройка шейдера
  if (sf::Shader::isAvailable()) {
    if (shineShader_.loadFromMemory(SHINE_SHADER_SRC,
                                    sf::Shader::Type::Fragment)) {
      shineShader_.setUniform("texture", sf::Shader::CurrentTexture);

      // Передаем точные размеры PNG в шейдер для попиксельной сетки
      if (logoTexture_) {
        shineShader_.setUniform(
            "texSize",
            sf::Vector2f(static_cast<float>(logoTexture_->getSize().x),
                         static_cast<float>(logoTexture_->getSize().y)));
      }
      shaderLoaded_ = true;
    }
  }

  menuFont_ = ResourceManager::getInstance().getFont(Assets::FONT_PRIMARY);
  if (!menuFont_) {
    menuFont_ = ResourceManager::getInstance().getFont(Assets::FONT_FALLBACK_1);
  }

  // initialize the fire effect
  fireEffect_ = std::make_unique<FireEffect>();
  if (fireEffect_) {
    fireEffect_->triggerFlash();
  }

  // 3. Загрузка доступных карт
  loadAvailableMaps();

  // 4. Настройка текста выбора карты
  if (menuFont_) {
    mapNameText_ = std::make_unique<sf::Text>(*menuFont_);
    mapNameText_->setCharacterSize(28);
    mapNameText_->setFillColor(sf::Color::Yellow);
    cycleMap(0);
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
  totalTime_ = 0.f;
  shinePos_ = -0.5;

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

  sf::View guiView(sf::FloatRect({0.f, 0.f}, {VIRTUAL_WIDTH, VIRTUAL_HEIGHT}));

  if (const auto *mouseMoved = event.getIf<sf::Event::MouseMoved>()) {
    sf::Vector2i pixelPos(mouseMoved->position.x, mouseMoved->position.y);
    mousePos_ = window_->mapPixelToCoords(pixelPos, guiView);
  }

  if (!logoAnimationComplete_) {
    if (event.is<sf::Event::KeyPressed>() ||
        event.is<sf::Event::MouseButtonPressed>()) {
      skipAnimation();
      return;
    }
    return;
  }

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

  // 1. Анимация подъема логотипа снизу
  if (logoSprite_ && !logoAnimationComplete_) {
    sf::Vector2f pos = logoSprite_->getPosition();
    if (pos.y > targetY_) {
      pos.y -= 750.f * deltaTime;
      if (pos.y <= targetY_) {
        pos.y = targetY_;
        logoAnimationComplete_ = true;
        shinePos_ = -0.2f;
      }
      logoSprite_->setPosition(pos);
    }
  }

  // 2. Микро-дрожь логотипа и цикличный пролет блика по UV-координатам
  if (logoSprite_ && logoAnimationComplete_) {
    float shakeX =
        std::sin(totalTime_ * 8.f) * 0.6f + std::cos(totalTime_ * 12.f) * 0.3f;
    float shakeY = std::cos(totalTime_ * 10.f) * 0.5f;
    logoSprite_->setPosition({VIRTUAL_CENTER_X + shakeX, targetY_ + shakeY});

    // Блик движется от -0.2 до 2.2 по диагонали текстуры
    shinePos_ += deltaTime * 0.9f;
    if (shinePos_ > 2.2f) {
      shinePos_ = -0.5f; // Пауза между пролетами
    }

    for (auto &button : buttons_) {
      if (button)
        button->update(mousePos_, deltaTime);
    }
  }
}

void MenuState::render(sf::RenderWindow &window) {
  if (!window_) {
    window_ = &window;
  }

  // Виртуальный View под VIRTUAL_WIDTH x VIRTUAL_HEIGHT с сохранением Aspect
  // Ratio
  sf::View guiView(sf::FloatRect({0.f, 0.f}, {VIRTUAL_WIDTH, VIRTUAL_HEIGHT}));

  float windowRatio = static_cast<float>(window.getSize().x) /
                      static_cast<float>(window.getSize().y);
  float virtualRatio = VIRTUAL_WIDTH / VIRTUAL_HEIGHT;
  sf::FloatRect viewport({0.f, 0.f}, {1.f, 1.f});

  if (windowRatio > virtualRatio) {
    float viewportWidth = virtualRatio / windowRatio;
    viewport.position.x = (1.f - viewportWidth) / 2.f;
    viewport.size.x = viewportWidth;
  } else if (windowRatio < virtualRatio) {
    float viewportHeight = windowRatio / virtualRatio;
    viewport.position.y = (1.f - viewportHeight) / 2.f;
    viewport.size.y = viewportHeight;
  }
  guiView.setViewport(viewport);
  window.setView(guiView);

  if (fireEffect_) {
    fireEffect_->render(window);
  }

  if (logoSprite_) {
    if (shaderLoaded_ && logoAnimationComplete_ && shinePos_ >= 0.0f &&
        shinePos_ <= 2.0f) {
      shineShader_.setUniform("shinePos", shinePos_);
      window.draw(*logoSprite_, &shineShader_);
    } else {
      window.draw(*logoSprite_);
    }
  }

  if (logoAnimationComplete_) {
    for (auto &button : buttons_) {
      if (button) {
        button->render(window);
      }
    }

    if (mapNameText_) {
      window.draw(*mapNameText_);
    }
  }
}

void MenuState::skipAnimation() {
  logoAnimationComplete_ = true;
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
