#pragma once
#include "../Core/GameState.h"
#include "../Core/StateManager.h"
#include "../UI/Button.h"
#include "../Utils/settings.h"
#include <SFML/Graphics.hpp>
#include <iostream>
#include <memory>
#include <vector>

class PauseState : public GameState {
public:
  void onEnter() override {
    std::cout << "=== PauseState: Game Paused ===" << std::endl;

    // Центр по горизонтали для виртуального сетки 1920x1200
    const float guiCenterX = 1920.f / 2.f;

    buttons_.reserve(3);
    buttons_.push_back(
        std::make_unique<Button>("Resume", sf::Vector2f(guiCenterX, 500.f)));
    buttons_.push_back(
        std::make_unique<Button>("Main Menu", sf::Vector2f(guiCenterX, 600.f)));
    buttons_.push_back(
        std::make_unique<Button>("Exit", sf::Vector2f(guiCenterX, 700.f)));

    exiting_ = false;
  }

  void onExit() override {
    std::cout << "=== PauseState: Exiting Pause Menu ===" << std::endl;
    buttons_.clear();
    window_ = nullptr;
  }

  void handleInput(const sf::Event &event) override {
    if (exiting_ || !window_)
      return;

    // Виртуальный View 1920x1200 для пересчета координат UI
    sf::View guiView(sf::FloatRect({0.f, 0.f}, {1920.f, 1200.f}));

    // 1. Движение мыши
    if (const auto *mouseMoved = event.getIf<sf::Event::MouseMoved>()) {
      sf::Vector2i pixelPos(mouseMoved->position.x, mouseMoved->position.y);
      mousePos_ = window_->mapPixelToCoords(pixelPos, guiView);
    }

    // 2. Клик мыши
    if (const auto *mousePressed =
            event.getIf<sf::Event::MouseButtonPressed>()) {
      if (mousePressed->button == sf::Mouse::Button::Left) {
        sf::Vector2i pixelPos(mousePressed->position.x,
                              mousePressed->position.y);
        sf::Vector2f clickPos = window_->mapPixelToCoords(pixelPos, guiView);

        for (auto &button : buttons_) {
          if (button && button->isClicked(clickPos)) {
            handleButtonClick(button->getName());
            return;
          }
        }
      }
    }

    // 3. Клавиатура (ESC)
    if (const auto *keyPressed = event.getIf<sf::Event::KeyPressed>()) {
      if (keyPressed->code == sf::Keyboard::Key::Escape) {
        exiting_ = true;
        stateManager_->popState();
        return;
      }
    }
  }

  void update(float deltaTime) override {
    for (auto &button : buttons_) {
      button->update(mousePos_, deltaTime);
    }
  }

  void render(sf::RenderWindow &window) override {
    if (!window_) {
      window_ = &window;
    }

    // Включаем виртуальное разрешение 1920x1200
    sf::View guiView(sf::FloatRect({0.f, 0.f}, {1920.f, 1200.f}));
    window.setView(guiView);

    // Полупрозрачный оверлей на весь виртуальный экран
    sf::RectangleShape overlay({1920.f, 1200.f});
    overlay.setFillColor(sf::Color(0, 0, 0, 180));
    window.draw(overlay);

    for (auto &button : buttons_) {
      button->render(window);
    }
  }

private:
  std::vector<std::unique_ptr<Button>> buttons_;
  sf::Vector2f mousePos_{0.f, 0.f};
  sf::RenderWindow *window_ = nullptr;
  bool exiting_ = false;

  void handleButtonClick(const std::string &buttonName) {
    if (buttonName == "Resume") {
      std::cout << ">>> Resuming Game <<<" << std::endl;
      exiting_ = true;
      stateManager_->popState();
    } else if (buttonName == "Main Menu") {
      std::cout << ">>> Returning to Main Menu <<<" << std::endl;
      exiting_ = true;
      stateManager_->clearAndSwitchTo("Menu");
    } else if (buttonName == "Exit") {
      std::cout << ">>> Exiting Game <<<" << std::endl;
      exiting_ = true;
      stateManager_->exitGame();
    }
  }
};
