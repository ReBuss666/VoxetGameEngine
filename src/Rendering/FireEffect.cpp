#include "../../include/Rendering/FireEffect.h"
#include "../../include/Utils/settings.h"
#include <algorithm>
#include <iostream>

FireEffect::FireEffect(int width, int height, int pixelSize)
    : width_(width), height_(height), pixelSize_(pixelSize),
      firePixels_(width * height, 0), vertices_(sf::PrimitiveType::Triangles),
      flashActive_(true), flashCounter_(0), flashDuration_(FLASH_DURATION),
      fuelEnabled_(true), rng_(std::random_device{}()), decayDist_(0, 1),
      driftDist_(-1, 1), flashDist_(20, 36) {
  initPalette();
  vertices_.resize(width * height * 6);

  // Запускаем стартовую вспышку при создании эффекта
  triggerFlash();

  std::cout << "[FireEffect] Initialized " << width << "x" << height << " ("
            << (width * height * 6) << " vertices) with initial flash"
            << std::endl;
}

void FireEffect::initPalette() {
  palette_[0] = sf::Color(0, 0, 0, 0);
  palette_[1] = sf::Color(7, 7, 7, 0);
  palette_[2] = sf::Color(15, 15, 15, 20);
  palette_[3] = sf::Color(31, 15, 7, 60);
  palette_[4] = sf::Color(47, 15, 7);
  palette_[5] = sf::Color(71, 31, 7);
  palette_[6] = sf::Color(87, 31, 7);
  palette_[7] = sf::Color(103, 31, 7);
  palette_[8] = sf::Color(119, 39, 7);
  palette_[9] = sf::Color(143, 47, 7);
  palette_[10] = sf::Color(159, 47, 7);
  palette_[11] = sf::Color(175, 55, 15);
  palette_[12] = sf::Color(191, 63, 15);
  palette_[13] = sf::Color(199, 63, 15);
  palette_[14] = sf::Color(223, 71, 15);
  palette_[15] = sf::Color(223, 71, 15);
  palette_[16] = sf::Color(215, 95, 15);
  palette_[17] = sf::Color(215, 79, 7);
  palette_[18] = sf::Color(215, 87, 7);
  palette_[19] = sf::Color(215, 95, 7);
  palette_[20] = sf::Color(215, 103, 15);
  palette_[21] = sf::Color(207, 111, 15);
  palette_[22] = sf::Color(207, 119, 15);
  palette_[23] = sf::Color(207, 127, 15);
  palette_[24] = sf::Color(199, 135, 23);
  palette_[25] = sf::Color(199, 143, 23);
  palette_[26] = sf::Color(199, 151, 31);
  palette_[27] = sf::Color(191, 159, 31);
  palette_[28] = sf::Color(191, 167, 39);
  palette_[29] = sf::Color(191, 175, 39);
  palette_[30] = sf::Color(191, 183, 47);
  palette_[31] = sf::Color(183, 191, 47);
  palette_[32] = sf::Color(183, 191, 55);
  palette_[33] = sf::Color(183, 191, 63);
  palette_[34] = sf::Color(183, 191, 71);
  palette_[35] = sf::Color(255, 255, 255);
  palette_[36] = sf::Color(255, 255, 255);
}

int FireEffect::getIndex(int x, int y) const {
  if (x < 0 || x >= width_ || y < 0 || y >= height_)
    return -1;
  return y * width_ + x;
}

void FireEffect::fuel() {
  for (int x = 0; x < width_; ++x) {
    int idx = getIndex(x, height_ - 1);
    if (idx != -1) {
      firePixels_[idx] = fuelEnabled_ ? 36 : 0;
    }
  }
}

void FireEffect::doFire(int x, int y) {
  int src_index = getIndex(x, y);
  if (src_index == -1)
    return;

  int heat = firePixels_[src_index];
  if (heat <= 0)
    return;

  int decay = decayDist_(rng_);
  int drift = driftDist_(rng_);

  int dest_x = x + drift;
  int dest_y = y - 1;

  int dest_index = getIndex(dest_x, dest_y);
  if (dest_index != -1) {
    firePixels_[dest_index] = std::max(0, heat - decay);
  }
}

void FireEffect::update() {
  if (flashActive_) {
    flashCounter_++;
    if (flashCounter_ > flashDuration_) {
      flashActive_ = false;
    }
  }

  fuel();

  for (int x = 0; x < width_; ++x) {
    for (int y = 1; y < height_; ++y) {
      doFire(x, y);
    }
  }

  updateVertices();
}

void FireEffect::updateVertices() {
  int vertexIndex = 0;

  for (int y = 0; y < height_; ++y) {
    for (int x = 0; x < width_; ++x) {
      int heat = firePixels_[getIndex(x, y)];

      sf::Color color;
      if (heat > 0) {
        color = palette_[std::min(heat, 36)];
      } else {
        color = sf::Color::Transparent;
      }

      float left = static_cast<float>(x * pixelSize_);
      float top = static_cast<float>(y * pixelSize_);
      float right = left + static_cast<float>(pixelSize_);
      float bottom = top + static_cast<float>(pixelSize_);

      vertices_[vertexIndex + 0].position = sf::Vector2f{left, top};
      vertices_[vertexIndex + 0].color = color;

      vertices_[vertexIndex + 1].position = sf::Vector2f{left, bottom};
      vertices_[vertexIndex + 1].color = color;

      vertices_[vertexIndex + 2].position = sf::Vector2f{right, top};
      vertices_[vertexIndex + 2].color = color;

      vertices_[vertexIndex + 3].position = sf::Vector2f{left, bottom};
      vertices_[vertexIndex + 3].color = color;

      vertices_[vertexIndex + 4].position = sf::Vector2f{right, bottom};
      vertices_[vertexIndex + 4].color = color;

      vertices_[vertexIndex + 5].position = sf::Vector2f{right, top};
      vertices_[vertexIndex + 5].color = color;

      vertexIndex += 6;
    }
  }
}

void FireEffect::triggerFlash() {
  for (int i = 0; i < static_cast<int>(firePixels_.size()); ++i) {
    firePixels_[i] = flashDist_(rng_);
  }
  flashActive_ = true;
  flashCounter_ = 0;
}

void FireEffect::render(sf::RenderWindow &window) { window.draw(vertices_); }

void FireEffect::enableFuel() { fuelEnabled_ = true; }

void FireEffect::disableFuel() { fuelEnabled_ = false; }

void FireEffect::resetFlash() {
  flashActive_ = true;
  flashCounter_ = 0;
}

bool FireEffect::isFlashActive() const { return flashActive_; }
