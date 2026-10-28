#pragma once
#include "../Core/GameState.h"
#include "../Core/ResourceManager.h"
#include "../Core/StateManager.h"
#include "../Rendering/FireEffect.h"
#include "../UI/Button.h"
#include "../Utils/settings.h"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <filesystem>
#include <memory>
#include <vector>

/**
 * @brief Main menu state with animated logo and fire effect
 */
class MenuState : public GameState {
public:
  MenuState() = default;
  ~MenuState() override = default;

  void onEnter() override;
  void onExit() override;
  void pause() override;
  void resume() override;
  void handleInput(const sf::Event &event) override;
  void update(float deltaTime) override;
  void render(sf::RenderWindow &window) override;

private:
  sf::Texture *logoTexture_ = nullptr;
  std::unique_ptr<sf::Sprite> logoSprite_;
  std::unique_ptr<sf::Sprite> shineSprite_;
  std::unique_ptr<FireEffect> fireEffect_;
  std::vector<std::unique_ptr<Button>> buttons_;
  sf::RenderWindow *window_ = nullptr;

  sf::Vector2f mousePos_{0.f, 0.f};
  float totalTime_ = 0.f;
  float shinePos_ = -1.0f;
  float targetY_ = 0.f;

  bool logoAnimationComplete_ = false;
  float buttonsFadeAlpha_ = 0.f;

  // Map selection
  std::vector<std::string> availableMaps_;
  int selectedMapIndex_ = 0;
  std::unique_ptr<sf::Text> mapNameText_;
  sf::Font *menuFont_ = nullptr;

  void skipAnimation();
  void handleButtonClick(const std::string &buttonName);
  void cycleMap(int direction);
  void loadAvailableMaps();
};
