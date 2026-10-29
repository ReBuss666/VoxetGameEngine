#pragma once

#include <SFML/Graphics.hpp>
#include <array>
#include <random>
#include <vector>

class FireEffect {
public:
  // Дефолтные параметры под сетку 320x200 (pixelSize = 6 идеально заполняет
  // VIRTUAL 1920x1200)
  explicit FireEffect(int width = 320, int height = 200, int pixelSize = 6);
  ~FireEffect() = default;

  void update();
  void render(sf::RenderWindow &window);
  void triggerFlash();

  void enableFuel();
  void disableFuel();

  bool isFlashActive() const;
  void resetFlash();

private:
  int width_, height_, pixelSize_;

  std::vector<int> firePixels_;
  std::array<sf::Color, 37> palette_;

  sf::VertexArray vertices_;

  bool flashActive_;
  int flashCounter_;
  int flashDuration_;

  bool fuelEnabled_;

  std::mt19937 rng_;
  std::uniform_int_distribution<int> decayDist_;
  std::uniform_int_distribution<int> driftDist_;
  std::uniform_int_distribution<int> flashDist_;

  void initPalette();
  void fuel();
  void doFire(int x, int y);
  void updateVertices();
  int getIndex(int x, int y) const;
};
