#include "../../include/Rendering/SectorRenderer.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

SectorRenderer::SectorRenderer(int screenWidth, int screenHeight)
    : screenWidth_(screenWidth), screenHeight_(screenHeight),
      renderDistance_(20.f), currentPitch_(0.0f), wallTexture_(nullptr) {
  columnVertices_.setPrimitiveType(sf::PrimitiveType::Triangles);
  columnVertices_.resize(screenWidth_ * 6 * 10);

  floorCeilingVertices_.setPrimitiveType(sf::PrimitiveType::Triangles);
  floorCeilingVertices_.resize(screenWidth_ * 12);

  std::cout << "[SectorRenderer] Created (" << screenWidth_ << "x"
            << screenHeight_ << ")" << std::endl;
}

void SectorRenderer::setTexture(const sf::Texture *texture) {
  wallTexture_ = texture;
}

void SectorRenderer::setRenderDistance(float distance) {
  renderDistance_ = distance;
}

const sf::Texture *
SectorRenderer::getTextureByName(const std::string &textureName) const {
  if (textureName.empty()) {
    return wallTexture_; // Use default if no texture specified
  }

  // Check cache first
  auto it = textureCache_.find(textureName);
  if (it != textureCache_.end()) {
    std::cout << "[Texture] Using cached texture: " << textureName << std::endl;
    return it->second;
  }

  // Load texture from ResourceManager
  std::string texPath = "assets/textures/walls/" + textureName;
  std::cout << "[Texture] Loading texture: " << texPath << std::endl;
  const sf::Texture *tex = ResourceManager::getInstance().getTexture(texPath);

  if (tex) {
    std::cout << "[Texture] Successfully loaded: " << textureName << std::endl;
    const_cast<sf::Texture *>(tex)->setRepeated(true);
    const_cast<sf::Texture *>(tex)->setSmooth(false);
    textureCache_[textureName] = tex;
    return tex;
  }

  // Fallback to default texture
  std::cout << "[Texture] FAILED to load: " << textureName
            << ", using wallTexture_" << std::endl;
  return wallTexture_;
}

void SectorRenderer::render(sf::RenderTarget &target, const SectorMap &map,
                            sf::Vector2f playerPos, float playerAngle,
                            const Sector *currentSector, float fov,
                            float playerHeight, float pitch) {

  std::cout << "[SectorRenderer] render() called" << std::endl;

  currentPitch_ = pitch;
  drawBackground(target, currentSector);

  std::cout << "[SectorRenderer] Background drawn" << std::endl;

  if (!currentSector) {
    std::cerr << "[SectorRenderer] WARNING: No current sector to render!"
              << std::endl;
    return;
  }

  // Clear texture batches for this frame
  textureBatches_.clear();

  // Подготовка контекста рендеринга
  RenderContext context;
  context.fov = fov;
  context.playerEyeZ = currentSector->getFloorHeight() + playerHeight;
  context.projectionDistance = (screenHeight_ / 2.0f) / std::tan(fov / 2.0f);
  float pitchOffset = pitch * screenHeight_ * 0.5f;
  context.screenCenterY = screenHeight_ / 2.0f - pitchOffset;

  // Рендеринг каждой колонки экрана
  for (int x = 0; x < screenWidth_; ++x) {
    float cameraX = 2.0f * x / static_cast<float>(screenWidth_) - 1.0f;
    float rayAngle = playerAngle + std::atan(cameraX * std::tan(fov / 2.0f));
    float cosCorrection = std::cos(rayAngle - playerAngle);
    sf::Vector2f rayDir(std::cos(rayAngle), std::sin(rayAngle));

    renderColumn(x, context, currentSector, playerPos, rayDir, cosCorrection);
  }

  std::cout << "[SectorRenderer] Collected " << textureBatches_.size()
            << " texture batches" << std::endl;

  // Draw all texture batches
  for (const auto &pair : textureBatches_) {
    const sf::Texture *tex = pair.first;
    const std::vector<sf::Vertex> &vertices = pair.second;

    std::cout << "[SectorRenderer] Batch has " << vertices.size() << " vertices"
              << std::endl;

    if (!vertices.empty() && tex) {
      sf::RenderStates states;
      states.texture = tex;
      target.draw(vertices.data(), vertices.size(),
                  sf::PrimitiveType::Triangles, states);
    }
  }

  std::cout << "[SectorRenderer] Finished rendering" << std::endl;
}

void SectorRenderer::renderColumn(int x, const RenderContext &context,
                                  const Sector *currentSector,
                                  sf::Vector2f playerPos, sf::Vector2f rayDir,
                                  float cosCorrection) {
  ClipRegion clip{0.0f, static_cast<float>(screenHeight_)};

  const Sector *sector = currentSector;
  sf::Vector2f rayOrigin = playerPos;
  float totalDistance = 0.0f;
  const int maxPortals = 8;

  for (int portalDepth = 0; portalDepth < maxPortals && sector; ++portalDepth) {
    const RaycastResult result = findClosestWall(*sector, rayOrigin, rayDir);

    if (!result.hit) {
      break;
    }

    totalDistance += result.distance;
    const float correctedDist = std::max(0.1f, totalDistance * cosCorrection);
    const sf::Vector2f hitPoint = rayOrigin + rayDir * result.distance;
    const WallGeometry geom =
        calculateWallGeometry(context, correctedDist, *sector, *result.wall,
                              hitPoint, result.hitSide);
    const sf::Color color =
        calculateWallColor(correctedDist, *sector, result.hitSide);

    if (result.wall->isPortal()) {
      Sector *neighbor = result.wall->getNeighborSector();
      if (!neighbor) {
        renderSolidWall(x, clip, geom, color, result.wall);
        break;
      }

      renderPortalWalls(x, clip, context, *sector, *neighbor, geom, color,
                        correctedDist, result.wall);

      const float WORLD_SCALE = TILE_SIZE;
      const float neighborFloorY =
          context.screenCenterY -
          (neighbor->getFloorHeight() - context.playerEyeZ) *
              context.projectionDistance * WORLD_SCALE / correctedDist;
      const float neighborCeilingY =
          context.screenCenterY -
          (neighbor->getCeilingHeight() - context.playerEyeZ) *
              context.projectionDistance * WORLD_SCALE / correctedDist;

      const ClipRegion newClip = calculatePortalClip(
          clip, geom.topY, geom.bottomY, neighborCeilingY, neighborFloorY);

      if (newClip.isEmpty()) {
        break;
      }

      clip = newClip;
      rayOrigin = hitPoint + rayDir * 0.1f;
      sector = neighbor;
    } else {
      renderSolidWall(x, clip, geom, color, result.wall);
      break;
    }
  }
}

RaycastResult SectorRenderer::findClosestWall(const Sector &sector,
                                              sf::Vector2f origin,
                                              sf::Vector2f direction) const {
  RaycastResult result;
  result.distance = renderDistance_ * TILE_SIZE;
  result.sector = &sector;

  const float MIN_DISTANCE = 0.01f;

  for (const auto &wall : sector.getWalls()) {
    float dist = 0.0f;
    bool side = false;

    if (checkRayWallIntersection(origin, direction, wall, dist, side)) {
      if (dist > MIN_DISTANCE && dist < result.distance) {
        result.distance = dist;
        result.wall = &wall;
        result.hitSide = side;
        result.hit = true;
      }
    }
  }

  return result;
}

WallGeometry SectorRenderer::calculateWallGeometry(
    const RenderContext &context, float distance, const Sector &sector,
    const Wall &wall, sf::Vector2f hitPoint, bool hitSide) const {
  const float WORLD_SCALE = TILE_SIZE;
  const float floorHeight = sector.getFloorHeight();
  const float ceilingHeight = sector.getCeilingHeight();

  WallGeometry geom;
  geom.bottomY = context.screenCenterY - (floorHeight - context.playerEyeZ) *
                                             context.projectionDistance *
                                             WORLD_SCALE / distance;
  geom.topY = context.screenCenterY - (ceilingHeight - context.playerEyeZ) *
                                          context.projectionDistance *
                                          WORLD_SCALE / distance;

  const float wallScreenHeight = std::max(0.1f, geom.bottomY - geom.topY);
  const float textureHeight =
      wallTexture_ ? static_cast<float>(wallTexture_->getSize().y) : 64.f;
  const float zoomFactor = 2.f;
  geom.screenScaleFactor = (textureHeight * zoomFactor) / wallScreenHeight;
  geom.textureX = calculateTextureX(wall, hitPoint);

  return geom;
}

sf::Color SectorRenderer::calculateWallColor(float distance,
                                             const Sector &sector,
                                             bool hitSide) const {
  const float worldDist = distance / TILE_SIZE;
  float brightness = std::max(0.3f, 1.0f - (worldDist / renderDistance_));

  if (hitSide) {
    brightness *= 0.7f;
  }

  brightness *= (sector.getLightLevel() / 255.0f);
  const uint8_t colorValue = static_cast<uint8_t>(255 * brightness);

  return sf::Color(colorValue, colorValue, colorValue);
}

void SectorRenderer::renderSolidWall(int x, const ClipRegion &clip,
                                     const WallGeometry &geom, sf::Color color,
                                     const Wall *wall) {
  const float wallTop = std::max(clip.top, geom.topY);
  const float wallBottom = std::min(clip.bottom, geom.bottomY);

  if (wallTop < wallBottom) {
    // If wall specified, try to use its texture
    if (wall) {
      const std::string &texName = wall->getMiddleTexture();
      const sf::Texture *currentTex = getTextureByName(texName);
      if (currentTex) {
        drawWallSegmentWithTexture(x, wallTop, wallBottom, geom, color,
                                   currentTex);
        return;
      }
    }
    drawWallSegment(x, wallTop, wallBottom, geom, color);
  }
}

void SectorRenderer::renderPortalWalls(int x, const ClipRegion &clip,
                                       const RenderContext &context,
                                       const Sector &currentSector,
                                       const Sector &neighborSector,
                                       const WallGeometry &geom,
                                       sf::Color color, float distance,
                                       const Wall *wall) {
  const float WORLD_SCALE = TILE_SIZE;
  const float neighborFloor = neighborSector.getFloorHeight();
  const float neighborCeiling = neighborSector.getCeilingHeight();

  const float neighborFloorY =
      context.screenCenterY - (neighborFloor - context.playerEyeZ) *
                                  context.projectionDistance * WORLD_SCALE /
                                  distance;
  const float neighborCeilingY =
      context.screenCenterY - (neighborCeiling - context.playerEyeZ) *
                                  context.projectionDistance * WORLD_SCALE /
                                  distance;

  // Верхняя стена (если потолок соседа ниже текущего)
  if (neighborCeiling < currentSector.getCeilingHeight()) {
    const float upperTop = std::max(clip.top, geom.topY);
    const float upperBottom = std::min(clip.bottom, neighborCeilingY);

    if (upperTop < upperBottom) {
      if (wall) {
        const std::string &upperTexName = wall->getUpperTexture();
        const sf::Texture *upperTex = getTextureByName(upperTexName);
        if (upperTex) {
          drawWallSegmentWithTexture(x, upperTop, upperBottom, geom, color,
                                     upperTex);
        } else {
          drawWallSegment(x, upperTop, upperBottom, geom, color);
        }
      } else {
        drawWallSegment(x, upperTop, upperBottom, geom, color);
      }
    }
  }

  // Нижняя стена (если пол соседа выше текущего)
  if (neighborFloor > currentSector.getFloorHeight()) {
    const float lowerTop = std::max(clip.top, neighborFloorY);
    const float lowerBottom = std::min(clip.bottom, geom.bottomY);

    if (lowerTop < lowerBottom) {
      if (wall) {
        const std::string &lowerTexName = wall->getLowerTexture();
        const sf::Texture *lowerTex = getTextureByName(lowerTexName);
        if (lowerTex) {
          drawWallSegmentWithTexture(x, lowerTop, lowerBottom, geom, color,
                                     lowerTex);
        } else {
          drawWallSegment(x, lowerTop, lowerBottom, geom, color);
        }
      } else {
        drawWallSegment(x, lowerTop, lowerBottom, geom, color);
      }
    }
  }
}

ClipRegion SectorRenderer::calculatePortalClip(const ClipRegion &currentClip,
                                               float currentCeilingY,
                                               float currentFloorY,
                                               float neighborCeilingY,
                                               float neighborFloorY) const {
  ClipRegion newClip;
  newClip.top =
      std::max(currentClip.top, std::max(currentCeilingY, neighborCeilingY));
  newClip.bottom =
      std::min(currentClip.bottom, std::min(currentFloorY, neighborFloorY));
  return newClip;
}

void SectorRenderer::drawWallSegmentWithTexture(int x, float top, float bottom,
                                                const WallGeometry &geom,
                                                sf::Color color,
                                                const sf::Texture *texture) {
  if (top >= bottom)
    return;
  if (!texture)
    texture = wallTexture_; // Fallback
  if (!texture) {
    std::cerr << "[SectorRenderer] WARNING: No texture available for rendering!"
              << std::endl;
    return; // Cannot render without texture
  }

  float textureWidth = static_cast<float>(texture->getSize().x);
  float texX = geom.textureX * textureWidth;

  float texYStart = (top - geom.topY) * geom.screenScaleFactor;
  float texYEnd = (bottom - geom.topY) * geom.screenScaleFactor;

  float left = static_cast<float>(x);
  float right = left + 1.f;

  // Add vertices to the batch for this specific texture
  auto &batch = textureBatches_[texture];

  // Reserve space if this is a new batch to avoid reallocations
  if (batch.empty()) {
    batch.reserve(screenWidth_ * 6); // Rough estimate
  }

  sf::Vertex v1, v2, v3, v4, v5, v6;

  v1.position = sf::Vector2f(left, top);
  v1.color = color;
  v1.texCoords = sf::Vector2f(texX, texYStart);

  v2.position = sf::Vector2f(left, bottom);
  v2.color = color;
  v2.texCoords = sf::Vector2f(texX, texYEnd);

  v3.position = sf::Vector2f(right, top);
  v3.color = color;
  v3.texCoords = sf::Vector2f(texX, texYStart);

  v4.position = sf::Vector2f(right, top);
  v4.color = color;
  v4.texCoords = sf::Vector2f(texX, texYStart);

  v5.position = sf::Vector2f(left, bottom);
  v5.color = color;
  v5.texCoords = sf::Vector2f(texX, texYEnd);

  v6.position = sf::Vector2f(right, bottom);
  v6.color = color;
  v6.texCoords = sf::Vector2f(texX, texYEnd);

  batch.push_back(v1);
  batch.push_back(v2);
  batch.push_back(v3);
  batch.push_back(v4);
  batch.push_back(v5);
  batch.push_back(v6);
}

void SectorRenderer::drawWallSegment(int x, float top, float bottom,
                                     const WallGeometry &geom,
                                     sf::Color color) {
  // Use wallTexture_ as fallback
  drawWallSegmentWithTexture(x, top, bottom, geom, color, wallTexture_);
}

void SectorRenderer::drawBackground(sf::RenderTarget &target,
                                    const Sector *sector) {
  float pitchOffset = currentPitch_ * screenHeight_ * 0.5f;
  float horizonY = screenHeight_ / 2.0f - pitchOffset;

  sf::RectangleShape sky;
  sky.setSize({static_cast<float>(screenWidth_), horizonY});
  sky.setFillColor(sf::Color(SKY_R, SKY_G, SKY_B));
  target.draw(sky);

  sf::VertexArray floorGradient(sf::PrimitiveType::Triangles, 6);

  sf::Color farColor(FLOOR_R / 4, FLOOR_G / 4, FLOOR_B / 4);
  sf::Color nearColor(FLOOR_R, FLOOR_G, FLOOR_B);

  const float screenWidthF = static_cast<float>(screenWidth_);
  const float screenHeightF = static_cast<float>(screenHeight_);

  floorGradient[0].position = sf::Vector2f(0.f, horizonY);
  floorGradient[0].color = farColor;

  floorGradient[1].position = sf::Vector2f(0.f, screenHeightF);
  floorGradient[1].color = nearColor;

  floorGradient[2].position = sf::Vector2f(screenWidthF, horizonY);
  floorGradient[2].color = farColor;

  floorGradient[3].position = sf::Vector2f(screenWidthF, horizonY);
  floorGradient[3].color = farColor;

  floorGradient[4].position = sf::Vector2f(0.f, screenHeightF);
  floorGradient[4].color = nearColor;

  floorGradient[5].position = sf::Vector2f(screenWidthF, screenHeightF);
  floorGradient[5].color = nearColor;

  target.draw(floorGradient);
}

bool SectorRenderer::checkRayWallIntersection(sf::Vector2f origin,
                                              sf::Vector2f direction,
                                              const Wall &wall, float &distance,
                                              bool &side) const {
  const sf::Vector2f wallDir = wall.getEnd() - wall.getStart();
  const float denom = direction.x * wallDir.y - direction.y * wallDir.x;

  // Параллельные линии
  const float EPSILON = 0.0001f;
  if (std::abs(denom) < EPSILON) {
    return false;
  }

  const sf::Vector2f diff = wall.getStart() - origin;
  const float t = (diff.x * wallDir.y - diff.y * wallDir.x) / denom;
  const float s = (diff.x * direction.y - diff.y * direction.x) / denom;

  // Проверка пересечения
  const bool validIntersection = (t > 0.0f) && (s >= 0.0f) && (s <= 1.0f);
  if (!validIntersection) {
    return false;
  }

  distance = t;
  const sf::Vector2f normal = wall.getNormal();
  side = (direction.x * normal.x + direction.y * normal.y) > 0;
  return true;
}

float SectorRenderer::calculateTextureX(const Wall &wall,
                                        sf::Vector2f hitPoint) const {
  const sf::Vector2f wallDir = wall.getEnd() - wall.getStart();
  const float wallLengthSq = wallDir.x * wallDir.x + wallDir.y * wallDir.y;

  const float EPSILON = 0.0001f;
  if (wallLengthSq < EPSILON) {
    return 0.0f;
  }

  const sf::Vector2f toHit = hitPoint - wall.getStart();
  const float t = (toHit.x * wallDir.x + toHit.y * wallDir.y) / wallLengthSq;
  const float wallLength = std::sqrt(wallLengthSq);
  const float distAlongWall = t * wallLength;

  float wallX = std::fmod(distAlongWall, TILE_SIZE) / TILE_SIZE;
  if (wallX < 0.0f) {
    wallX += 1.0f;
  }

  return wallX;
}
