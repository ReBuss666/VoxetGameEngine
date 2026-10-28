#pragma once
#include "../Core/GameState.h"
#include "../Core/ResourceManager.h"
#include "../Core/StateManager.h"
#include "../Entities/player.h"
#include "../Rendering/SectorRenderer.h"
#include "../Utils/settings.h"
#include "../World/MapLoader.h"
#include "../World/SectorMap.h"
#include "../World/Testmapbuilder.h"
#include "../weapon/pistol.h"
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <cmath>
#include <iostream>
#include <memory>

class PlayState : public GameState {
public:
  // Constructor requires map file path
  explicit PlayState(const std::string &mapPath) : mapFilePath_(mapPath) {}

  void onEnter() override {
    std::cout << "=== PlayState (Sector Engine): Entering ===" << std::endl;

    std::cout << "[PlayState] Creating render texture " << INTERNAL_WIDTH << "x"
              << INTERNAL_HEIGHT << std::endl;
    if (!renderTexture_.resize({INTERNAL_WIDTH, INTERNAL_HEIGHT})) {
      std::cerr << "ERROR: Failed to create render texture!" << std::endl;
    }
    std::cout << "[PlayState] Render texture created successfully" << std::endl;
    renderTexture_.setSmooth(false);

    std::cout << "[PlayState] Creating render sprite" << std::endl;
    renderSprite_ = std::make_unique<sf::Sprite>(renderTexture_.getTexture());
    std::cout << "[PlayState] Render sprite created successfully" << std::endl;

    std::cout << "[PlayState] Loading sector map..." << std::endl;
    std::cout << "[PlayState] Map path: " << mapFilePath_ << std::endl;

    // Try to load from JSON file directly
    auto result = MapLoader::loadFromFile(mapFilePath_, sectorMap_, 1.0f);

    if (!result.success) {
      std::cerr << "[PlayState] Could not load map from file: " << result.error
                << std::endl;
      std::cout << "[PlayState] Using TestMapBuilder as fallback..."
                << std::endl;

      // Fallback to programmatic map builders based on filename
      if (mapFilePath_.find("simple_step") != std::string::npos) {
        sectorMap_ = TestMapBuilder::buildSimpleStepMap();
      } else if (mapFilePath_.find("window_demo") != std::string::npos) {
        sectorMap_ = TestMapBuilder::buildWindowMap();
      } else if (mapFilePath_.find("complex") != std::string::npos) {
        sectorMap_ = TestMapBuilder::buildComplexMap();
      } else {
        // Default fallback
        sectorMap_ = TestMapBuilder::buildSimpleStepMap();
      }
    } else {
      std::cout << "[PlayState] Successfully loaded map from: " << mapFilePath_
                << std::endl;
      mapTextures_ = result.textureList;
    }

    if (!sectorMap_.validate()) {
      std::cerr << "ERROR: Sector map validation failed!" << std::endl;
    }

    // Spawn player in center of first sector, or at default position
    sf::Vector2f spawnPos(128.0f, 128.0f); // Default spawn
    if (Sector *firstSector = sectorMap_.getSector(1)) {
      spawnPos = firstSector->getCenter();
      std::cout << "[PlayState] Spawning at sector 0 center: " << spawnPos.x
                << ", " << spawnPos.y << std::endl;
    }
    player_ = std::make_unique<Player>(spawnPos);

    updatePlayerSector();

    if (currentSector_) {
      std::cout << "[PlayState] Player starts in sector "
                << currentSector_->getId() << std::endl;
    } else {
      std::cerr << "WARNING: Player not in any sector!" << std::endl;
    }

    sectorRenderer_ =
        std::make_unique<SectorRenderer>(INTERNAL_WIDTH, INTERNAL_HEIGHT);
    sectorRenderer_->setRenderDistance(RAYCASTER_RENDER_DISTANCE);

    // Load textures from JSON or default
    if (!mapTextures_.empty()) {
      std::cout << "[PlayState] Loading textures from map..." << std::endl;
      for (const auto &texName : mapTextures_) {
        if (!texName.empty()) {
          std::string texPath = "assets/textures/walls/" + texName;
          sf::Texture *tex = ResourceManager::getInstance().getTexture(texPath);
          if (tex) {
            tex->setRepeated(true);
            tex->setSmooth(false);
            std::cout << "[PlayState] Loaded texture: " << texPath << std::endl;
          }
        }
      }
    }

    // Set default wall texture
    wallTexture_ =
        ResourceManager::getInstance().getTexture(Assets::WALL_TEXTURE);
    if (wallTexture_) {
      wallTexture_->setRepeated(true);
      wallTexture_->setSmooth(false);
      sectorRenderer_->setTexture(wallTexture_);
      std::cout << "[PlayState] Default wall texture: " << Assets::WALL_TEXTURE
                << " (" << wallTexture_->getSize().x << "x"
                << wallTexture_->getSize().y << ")" << std::endl;
    } else {
      std::cerr << "WARNING: Wall texture not found at: "
                << Assets::WALL_TEXTURE << std::endl;
      std::cerr << "          Walls will appear white. Check the file path."
                << std::endl;
    }

    mode3D_ = true;
    mouseLocked_ = false;

    std::cout << "=== PlayState: Ready ===" << std::endl;
    std::cout << "    Controls:" << std::endl;
    std::cout << "    - WASD: Move" << std::endl;
    std::cout << "    - Mouse/Arrow Keys: Look around" << std::endl;
    std::cout << "    - Space: Jump" << std::endl;
    std::cout << "    - Shift: Sprint" << std::endl;
    std::cout << "    - C/Ctrl: Crouch" << std::endl;
    std::cout << "    - TAB: Toggle 2D/3D view" << std::endl;
    std::cout << "    - M: Lock/unlock mouse" << std::endl;
    std::cout << "    - ESC: Pause menu" << std::endl;

    gun_ = std::make_unique<Gun>(Assets::PISTOL_FIRE_ANIM);
  }

  void onExit() override {
    if (window_) {
      window_->setMouseCursorVisible(true);
      window_->setMouseCursorGrabbed(false);
    }

    sectorRenderer_.reset();
    player_.reset();
    renderSprite_.reset();
    wallTexture_ = nullptr;
    currentSector_ = nullptr;
  }

  void handleInput(const sf::Event &event) override {
    if (const auto *keyPressed = event.getIf<sf::Event::KeyPressed>()) {
      if (keyPressed->code == sf::Keyboard::Key::Escape) {
        if (mouseLocked_) {
          unlockMouse();
        } else {
          if (stateManager_)
            stateManager_->pushState("Pause");
        }
      }
      if (keyPressed->code == sf::Keyboard::Key::Tab) {
        mode3D_ = !mode3D_;
        std::cout << "[PlayState] Switched to " << (mode3D_ ? "3D" : "2D")
                  << " mode" << std::endl;
        if (!mode3D_) {
          unlockMouse();
        }
      }
      if (keyPressed->code == sf::Keyboard::Key::M && mode3D_) {
        mouseLocked_ = !mouseLocked_;
        if (mouseLocked_ && player_) {
          lockMouse();
        } else {
          unlockMouse();
        }
      }

      if (keyPressed->code == sf::Keyboard::Key::Space && player_) {
        player_->jump();
      }

      if (keyPressed->code == sf::Keyboard::Key::C && player_) {
        player_->toggleCrouch();
      }
    }

    if (player_) {
      player_->setSprinting(
          sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) ||
          sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift));

      if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl)) {
        player_->setCrouching(true);
      } else {
        player_->setCrouching(false);
      }
    }

    if (const auto *mouseMoved = event.getIf<sf::Event::MouseMoved>()) {
      if (mouseLocked_ && mode3D_ && window_) {
        sf::Vector2u windowSize = window_->getSize();
        sf::Vector2i center(windowSize.x / 2, windowSize.y / 2);

        float deltaX = static_cast<float>(mouseMoved->position.x - center.x);
        float deltaY = static_cast<float>(mouseMoved->position.y - center.y);

        if (std::abs(deltaX) > 0.5f) {
          mouseRotation_ += deltaX * MOUSE_SENSITIVITY;
        }

        if (std::abs(deltaY) > 0.5f) {
          mousePitch_ += deltaY * MOUSE_SENSITIVITY_Y;
        }

        if (std::abs(deltaX) > 5.f || std::abs(deltaY) > 5.f) {
          sf::Mouse::setPosition(center, *window_);
        }
      }
    }

    if (const auto *mousePressed =
            event.getIf<sf::Event::MouseButtonPressed>()) {
      if (mode3D_ && !mouseLocked_ &&
          mousePressed->button == sf::Mouse::Button::Left) {
        lockMouse();
      }
      // Shooting with left mouse button when mouse is locked
      if (mode3D_ && mouseLocked_ &&
          mousePressed->button == sf::Mouse::Button::Left) {
        if (gun_) {
          gun_->shoot();
        }
      }
    }
  }

  void update(float deltaTime) override {
    if (player_) {
      float currentFloorHeight = 0.0f;
      if (currentSector_) {
        currentFloorHeight = currentSector_->getFloorHeight();
      }

      player_->updateVertical(deltaTime, currentFloorHeight);

      bool wasMoving = updatePlayerMovement(deltaTime);

      player_->updateHeadBob(deltaTime, wasMoving);

      player_->handleKeyboardRotation(deltaTime);

      if (mouseLocked_ && std::abs(mouseRotation_) > 0.001f) {
        float currentAngle = player_->getViewAngle();
        player_->setViewAngle(currentAngle + mouseRotation_);
        lastMouseDeltaX_ = mouseRotation_ * 1000.f;
        mouseRotation_ = 0.f;
      } else {
        lastMouseDeltaX_ *= 0.9f;
      }

      if (mouseLocked_ && std::abs(mousePitch_) > 0.001f) {
        float currentPitch = player_->getPitchAngle();
        player_->setPitchAngle(currentPitch + mousePitch_);
        lastMouseDeltaY_ = mousePitch_ * 1000.f;
        mousePitch_ = 0.f;
      } else {
        lastMouseDeltaY_ *= 0.9f;
      }

      updatePlayerSector();

      if (currentSector_) {
        float floorDiff =
            currentSector_->getFloorHeight() - player_->getVerticalPos();
        if (floorDiff > 0 && floorDiff <= MAX_STEP_HEIGHT &&
            player_->isGrounded()) {
          player_->setVerticalPos(currentSector_->getFloorHeight());
        }
      }
    }

    // Update gun animation, bob and sway
    if (gun_ && player_) {
      bool isMoving = player_->isMoving();
      bool isSprinting =
          sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) ||
          sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);
      gun_->update(deltaTime, isMoving, isSprinting, lastMouseDeltaX_,
                   lastMouseDeltaY_);
    }
  }

  void render(sf::RenderWindow &window) override {
    if (!window_) {
      window_ = &window;
    }

    if (mode3D_) {
      // 1. Отрисовка 3D сцены в низкополигональный буфер кадра
      renderTexture_.clear(sf::Color::Black);

      if (sectorRenderer_ && player_ && currentSector_) {
        float eyeHeight = player_->getEyeHeight();
        float pitch = player_->getPitchAngle();
        sectorRenderer_->render(renderTexture_, sectorMap_,
                                player_->getPosition(), player_->getViewAngle(),
                                currentSector_, FOV_RADIANS, eyeHeight, pitch);
      } else if (!currentSector_) {
        std::cerr << "[PlayState] WARNING: Player not in any sector!"
                  << std::endl;
      }
      renderTexture_.display();

      // 2. Вывод 3D сцены на полный экран монитора
      window.setView(window.getDefaultView());
      if (renderSprite_) {
        float scaleX = static_cast<float>(window.getSize().x) /
                       static_cast<float>(INTERNAL_WIDTH);
        float scaleY = static_cast<float>(window.getSize().y) /
                       static_cast<float>(INTERNAL_HEIGHT);
        renderSprite_->setScale({scaleX, scaleY});
        window.draw(*renderSprite_);
      }

      // 3. Включаем виртуальный GUI View под VIRTUAL_WIDTH x VIRTUAL_HEIGHT
      sf::View guiView(
          sf::FloatRect({0.f, 0.f}, {VIRTUAL_WIDTH, VIRTUAL_HEIGHT}));
      window.setView(guiView);

      drawCrosshair(window);

      if (gun_) {
        gun_->render(window, VIRTUAL_WIDTH, VIRTUAL_HEIGHT);
      }

      drawHUD(window);
    } else {
      // 2D режим
      sf::View mapView;
      sf::Vector2f playerPos = player_->getPosition();
      float viewSize = 800.0f;
      mapView.setCenter(playerPos);
      mapView.setSize(
          {viewSize, viewSize * static_cast<float>(window.getSize().y) /
                         static_cast<float>(window.getSize().x)});
      window.setView(mapView);

      sectorMap_.render2D(window, player_->getPosition(), 1.0f);

      if (player_) {
        player_->draw(window);
        drawViewDirection(window);
      }

      if (currentSector_) {
        sf::FloatRect bounds = currentSector_->getBounds();
        sf::RectangleShape highlight;
        highlight.setSize({bounds.size.x, bounds.size.y});
        highlight.setPosition({bounds.position.x, bounds.position.y});
        highlight.setFillColor(sf::Color(255, 255, 0, 30));
        highlight.setOutlineColor(sf::Color::Yellow);
        highlight.setOutlineThickness(2.0f);
        window.draw(highlight);
      }

      // Возврат на GUI View для отрисовки HUD в 2D режиме
      sf::View guiView(
          sf::FloatRect({0.f, 0.f}, {VIRTUAL_WIDTH, VIRTUAL_HEIGHT}));
      window.setView(guiView);
      drawHUD(window);
    }
  }

private:
  std::unique_ptr<Player> player_;
  SectorMap sectorMap_;
  Sector *currentSector_ = nullptr;
  std::unique_ptr<SectorRenderer> sectorRenderer_;
  std::unique_ptr<Gun> gun_;

  sf::RenderTexture renderTexture_;
  std::unique_ptr<sf::Sprite> renderSprite_;

  sf::Texture *wallTexture_ = nullptr;

  // Map loading
  std::string mapFilePath_;
  std::vector<std::string> mapTextures_;
  sf::RenderWindow *window_ = nullptr;

  bool mode3D_ = true;
  bool mouseLocked_ = false;
  float mouseRotation_ = 0.f;
  float mousePitch_ = 0.f;
  float lastMouseDeltaX_ = 0.f; // Для передачи в Gun sway
  float lastMouseDeltaY_ = 0.f;

  void updatePlayerSector() {
    Sector *newSector = sectorMap_.findSectorAt(player_->getPosition());

    if (newSector != currentSector_) {
      if (newSector) {
        std::cout << "[PlayState] Player entered sector " << newSector->getId()
                  << std::endl;
      } else {
        std::cout << "[PlayState] WARNING: Player outside all sectors!"
                  << std::endl;
      }
      currentSector_ = newSector;
    }
  }

  bool updatePlayerMovement(float deltaTime) {
    sf::Vector2f inputDir{0.f, 0.f};

    if (mode3D_) {
      float viewAngle = player_->getViewAngle();

      if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) {
        inputDir.x += std::cos(viewAngle);
        inputDir.y += std::sin(viewAngle);
      }
      if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) {
        inputDir.x -= std::cos(viewAngle);
        inputDir.y -= std::sin(viewAngle);
      }
      if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) {
        inputDir.x += std::cos(viewAngle - M_PI / 2.f);
        inputDir.y += std::sin(viewAngle - M_PI / 2.f);
      }
      if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) {
        inputDir.x += std::cos(viewAngle + M_PI / 2.f);
        inputDir.y += std::sin(viewAngle + M_PI / 2.f);
      }
    } else {
      if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W))
        inputDir.y -= 1.f;
      if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S))
        inputDir.y += 1.f;
      if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A))
        inputDir.x -= 1.f;
      if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D))
        inputDir.x += 1.f;
    }

    bool hasInput = (inputDir.x != 0.f || inputDir.y != 0.f);

    if (hasInput) {
      float length =
          std::sqrt(inputDir.x * inputDir.x + inputDir.y * inputDir.y);
      inputDir /= length;
    }

    float speedMultiplier = player_->getSpeedMultiplier();
    float currentSpeed = PLAYER_SPEED * speedMultiplier;

    sf::Vector2f currentPos = player_->getPosition();
    sf::Vector2f newPos = currentPos + inputDir * currentSpeed * deltaTime;

    bool actuallyMoved = false;

    bool isInAir = !player_->isGrounded();
    bool blocked =
        sectorMap_.isBlocked(currentPos, newPos, PLAYER_WIDTH / 2.0f, isInAir);

    if (!blocked && hasInput) {
      player_->setPosition(newPos);
      actuallyMoved = true;
    } else if (hasInput) {
      sf::Vector2f slideX = sf::Vector2f(newPos.x, currentPos.y);
      sf::Vector2f slideY = sf::Vector2f(currentPos.x, newPos.y);

      if (!sectorMap_.isBlocked(currentPos, slideX, PLAYER_WIDTH / 2.0f,
                                isInAir)) {
        player_->setPosition(slideX);
        actuallyMoved = true;
      } else if (!sectorMap_.isBlocked(currentPos, slideY, PLAYER_WIDTH / 2.0f,
                                       isInAir)) {
        player_->setPosition(slideY);
        actuallyMoved = true;
      }
    }

    return actuallyMoved;
  }

  void lockMouse() {
    if (!window_)
      return;

    mouseLocked_ = true;
    mouseRotation_ = 0.f;

    window_->setMouseCursorVisible(false);
    window_->setMouseCursorGrabbed(true);

    sf::Vector2u windowSize = window_->getSize();
    sf::Mouse::setPosition(sf::Vector2i(windowSize.x / 2, windowSize.y / 2),
                           *window_);

    if (player_) {
      player_->resetMouseTracking();
    }

    std::cout << "[PlayState] Mouse locked" << std::endl;
  }

  void unlockMouse() {
    if (!window_)
      return;

    mouseLocked_ = false;
    window_->setMouseCursorVisible(true);
    window_->setMouseCursorGrabbed(false);

    std::cout << "[PlayState] Mouse unlocked" << std::endl;
  }

  void drawCrosshair(sf::RenderWindow &window) {
    sf::RectangleShape hLine({20.f, 2.f});
    hLine.setPosition({VIRTUAL_CENTER_X - 10.f, VIRTUAL_CENTER_Y - 1.f});
    hLine.setFillColor(sf::Color::White);

    sf::RectangleShape vLine({2.f, 20.f});
    vLine.setPosition({VIRTUAL_CENTER_X - 1.f, VIRTUAL_CENTER_Y - 10.f});
    vLine.setFillColor(sf::Color::White);

    window.draw(hLine);
    window.draw(vLine);
  }

  void drawViewDirection(sf::RenderWindow &window) {
    if (!player_)
      return;
    sf::Vector2f pos = player_->getPosition();
    float angle = player_->getViewAngle();
    sf::Vector2f endPos(pos.x + std::cos(angle) * 50.f,
                        pos.y + std::sin(angle) * 50.f);
    sf::Vertex line[2];
    line[0].position = pos;
    line[0].color = sf::Color::Yellow;
    line[1].position = endPos;
    line[1].color = sf::Color::Yellow;
    window.draw(line, 2, sf::PrimitiveType::Lines);
  }

  void drawHUD(sf::RenderWindow &window) {
    static sf::Font *font =
        ResourceManager::getInstance().getFont(Assets::FONT_PRIMARY);
    if (font) {
      sf::Text text(*font);

      // Mode and mouse status
      std::string modeText = mode3D_ ? "3D Mode (Sector Engine)" : "2D Mode";
      std::string mouseText =
          mouseLocked_ ? " | Mouse: LOCKED [ESC]" : " | Click/[M] to lock";

      // Player state info
      std::string stateText = "";
      if (player_) {
        if (player_->isSprinting())
          stateText += " [SPRINT]";
        if (player_->isCrouching())
          stateText += " [CROUCH]";
        if (player_->isJumping())
          stateText += " [JUMP]";
        if (!player_->isGrounded())
          stateText += " [AIR]";
      }

      // Current sector info
      std::string sectorText = "";
      if (currentSector_) {
        sectorText =
            "\nSector " + std::to_string(currentSector_->getId()) +
            " | Floor: " + std::to_string(currentSector_->getFloorHeight()) +
            "m | Ceiling: " +
            std::to_string(currentSector_->getCeilingHeight()) + "m";
        if (player_) {
          sectorText += " | Eye: " +
                        std::to_string(player_->getEyeHeight()).substr(0, 4) +
                        "m";
        }
      } else {
        sectorText = "\nWARNING: Outside all sectors!";
      }

      // Controls hint
      std::string controlsText =
          "\n[Space] Jump | [Shift] Sprint | [C/Ctrl] Crouch";

      text.setString(modeText + mouseText + stateText + sectorText +
                     controlsText);
      text.setCharacterSize(20);
      text.setPosition({10.f, 10.f});
      text.setFillColor(sf::Color::White);

      // Add background for readability
      sf::FloatRect textBounds = text.getGlobalBounds();
      sf::RectangleShape background;
      background.setSize({textBounds.size.x + 20.f, textBounds.size.y + 20.f});
      background.setPosition(
          {textBounds.position.x - 10.f, textBounds.position.y - 10.f});
      background.setFillColor(sf::Color(0, 0, 0, 150));

      window.draw(background);
      window.draw(text);
    }
  }
};
