#include "MouseControl.h"

void MouseControl::MouseBox(const AssetManager_Ptr &assetManager,
                            Renderer &renderer, SDL_Rect &mouseBox,
                            SDL_Rect &camera, bool collider) {
  // If Grid Snap is enabled, snap the tile to the next grid location
  if (mGridSnap) {
    mMousePosGrid.x = mMousePosX * mGridSize;
    mMousePosGrid.y = mMousePosY * mGridSize;

    if (mMousePosX >= 0)
      mMousePosGrid.x = mMousePosX / mGridSize;
    if (mMousePosY >= 0)
      mMousePosGrid.y = mMousePosY / mGridSize;

    mouseBox.x = std::round(mMousePosGrid.x * mGridSize * mZoom) - camera.x;
    mouseBox.y = std::round(mMousePosGrid.y * mGridSize * mZoom) - camera.y;
  } else // Float the center of the tile on the mouse
  {
    mouseBox.x = (mMousePosX * mZoom - camera.x -
                  (mMouseRect.x * mTransformComponent.scale.x * mZoom) / 2);
    mouseBox.y = (mMousePosY * mZoom - camera.y -
                  (mMouseRect.y * mTransformComponent.scale.y * mZoom) / 2);
  }

  // Do not draw the mouse box image outside of the mouse bounds
  if (MouseOutOfBounds())
    return;

  SDL_Rect srcRect = {mSpriteComponent.srcRect.x, mSpriteComponent.srcRect.y,
                      mMouseRect.x, mMouseRect.y};

  SDL_Rect dstRect = {mouseBox.x, mouseBox.y,
                      std::round(mouseBox.w * mMouseRect.x *
                                 mTransformComponent.scale.x * mZoom),
                      std::round(mouseBox.h * mMouseRect.y *
                                 mTransformComponent.scale.y * mZoom)};

  // If not a collider, draw the selected tile image
  if (!collider) {
    SDL_RenderCopyEx(renderer.get(),
                     assetManager->GetTexture(mSpriteComponent.assetId).get(),
                     &srcRect, &dstRect, mTransformComponent.rotation, NULL,
                     mSpriteComponent.flip);
  } else {
    SDL_SetRenderDrawColor(renderer.get(), 255, 0, 0, 100);
    SDL_RenderFillRect(renderer.get(), &dstRect);
    SDL_RenderDrawRect(renderer.get(), &dstRect);
  }
}

bool MouseControl::FastErase(const glm::vec2 &pos) {
  if (!RightButtonDown())
    return false;
  if (mGridSnap)
    return (pos.x != mPrevMousePosErase.x || pos.y != mPrevMousePosErase.y);
  else
    return true; // continuous erase while dragging in free mode
}

bool MouseControl::FastTile(const glm::vec2 &pos) {
  // This is only used while in gridsnap maode
  if (mGridSnap) {
    if ((pos.x != mPrevMousePos.x || pos.y != mPrevMousePos.y) &&
        LeftButtonDown())
      return true;
    else
      return false;
  } else
    return false;
}

MouseControl::MouseControl()
    : mMouseRect(glm::vec2(16, 16)), mMousePosX(0), mMousePosY(0),
      mMousePosGrid(glm::vec2(0)),
      mPrevMousePos(glm::vec2(mMousePosX, mMousePosY)),
      mPrevMousePosErase(glm::vec2(-1, -1)),
      mMousePosScreen(glm::vec2(0)), mZoom(0), mGridSize(16), mPanX(0),
      mPanY(0), mMostRecentTileId(-1), mIsCollider(false), mIsAnimated(false),
      mGridSnap(true), mOverImGuiWindow(false), mLeftPressed(false),
      mRightPressed(false), mTileAdded(false), mTileRemoved(false),
      mSpriteComponent(), mTransformComponent(), mRemovedTransformComponent(),
      mBoxColliderComponent(), mRemovedBoxComponent(), mAnimationComponent(),
      mRemovedAnimationComponent() {}

void MouseControl::CreateTile(const AssetManager_Ptr &assetManager,
                              Renderer &renderer, SDL_Rect &mouseBox,
                              SDL_Rect &camera, SDL_Event &event) {
  // Refresh mouse position with the current frame's data so tile placement
  // matches exactly where the cursor is right now, not one frame behind.
  {
    int rawX, rawY;
    SDL_GetMouseState(&rawX, &rawY);
    if (mZoom != 0.0f) {
      mMousePosX = static_cast<int>((rawX + camera.x) / mZoom);
      mMousePosY = static_cast<int>((rawY + camera.y) / mZoom);
      mMousePosScreen.x = mMousePosX;
      mMousePosScreen.y = mMousePosY;
    }
  }

  // Draw the Mouse Box Image, this follows the mouse
  MouseBox(assetManager, renderer, mouseBox, camera, false);

  // Do not create tiles outside of the mouse bounds
  if (MouseOutOfBounds())
    return;

  // This is used in the FastTile function for comparisons
  glm::vec2 pos = glm::vec2(mouseBox.x + camera.x / mGridSize,
                            mouseBox.y + camera.y / mGridSize);

  // Set the transform position to the current mousebox position
  mTransformComponent.position =
      glm::vec2(mouseBox.x + camera.x, mouseBox.y + camera.y);

  // Reset the mouse press if not pressed
  if (!LeftButtonDown())
    mLeftPressed = false;
  if (!RightButtonDown())
    mRightPressed = false;

  if ((event.type == SDL_MOUSEBUTTONDOWN || LeftButtonDown() ||
       RightButtonDown()) &&
      !mOverImGuiWindow) {
    // If the left mouse button is pressed, create a tile/collider at that
    // location
    if ((event.button.button == SDL_BUTTON_LEFT && !mLeftPressed) ||
        FastTile(pos)) {
      // Update Grid values
      int mGridX = static_cast<int>(mMousePosScreen.x) / mGridSize;
      int mGridY = static_cast<int>(mMousePosScreen.y) / mGridSize;

      if (mGridSnap) {
        mTransformComponent.position.x = mGridX * mGridSize;
        mTransformComponent.position.y = mGridY * mGridSize;
      } else {
        mTransformComponent.position.x =
            static_cast<int>(mMousePosScreen.x -
                             (mMouseRect.x * mTransformComponent.scale.x / 2));
        mTransformComponent.position.y =
            static_cast<int>(mMousePosScreen.y -
                             (mMouseRect.y * mTransformComponent.scale.y / 2));
      }

      // Remove any existing tile at this grid position on the same z-layer
      if (Registry::Instance().DoesGroupExist("tiles")) {
        glm::vec2 newPos(mTransformComponent.position.x,
                         mTransformComponent.position.y);
        for (auto &existing : Registry::Instance().GetEntitiesByGroup("tiles")) {
          const auto &t = existing.GetComponent<TransformComponent>();
          const auto &s = existing.GetComponent<SpriteComponent>();
          if (s.zIndex == mSpriteComponent.zIndex &&
              static_cast<int>(t.position.x) == static_cast<int>(newPos.x) &&
              static_cast<int>(t.position.y) == static_cast<int>(newPos.y)) {
            existing.Kill();
          }
        }
      }

      // Create a new tile entity and add the necessary components
      Entity tile = Registry::Instance().CreateEntity();
      tile.Group("tiles");
      tile.AddComponent<TransformComponent>(
          glm::vec2(mTransformComponent.position.x,
                    mTransformComponent.position.y),
          mTransformComponent.scale, mTransformComponent.rotation);

      tile.AddComponent<SpriteComponent>(
          mSpriteComponent.assetId, mSpriteComponent.width,
          mSpriteComponent.height, mSpriteComponent.zIndex,
          mSpriteComponent.isFixed, mSpriteComponent.srcRect.x,
          mSpriteComponent.srcRect.y, mSpriteComponent.offset);

      // If the tile is a box collider, Add a BoxColliderComponent
      if (mIsCollider) {
        tile.AddComponent<BoxColliderComponent>(mBoxColliderComponent.width,
                                                mBoxColliderComponent.height,
                                                mBoxColliderComponent.offset);
      }

      if (mIsAnimated) {
        tile.AddComponent<AnimationComponent>(
            mAnimationComponent.numFrames, mAnimationComponent.frameSpeedRate,
            mAnimationComponent.vertical, mAnimationComponent.isLooped,
            mAnimationComponent.frameOffset);
      }

      // Get Most Recent Tile Id
      mMostRecentTileId = tile.GetId();

      mLeftPressed = true;
      mTileAdded = true;
      // This is used for Creating tiles faster
      mPrevMousePos.x = pos.x;
      mPrevMousePos.y = pos.y;
    }

    // If the right mouse button is pressed/held, remove the tile/collider at
    // that location (drag-erasing supported via FastErase)
    if (!mOverImGuiWindow &&
        ((event.type == SDL_MOUSEBUTTONDOWN &&
          event.button.button == SDL_BUTTON_RIGHT && !mRightPressed) ||
         FastErase(pos))) {
      if (!Registry::Instance().DoesGroupExist("tiles"))
        return;

      // This value is used as a tolerance area so the mouse does not need to be
      // exactly on the tile to remove it
      glm::vec2 subtract =
          glm::vec2((mMouseRect.x * mTransformComponent.scale.x) / 2,
                    (mMouseRect.y * mTransformComponent.scale.y) / 2);

      // Get all the entities from the group "tiles"
      auto entities = Registry::Instance().GetEntitiesByGroup("tiles");

      // Loop through tiles and remove the one that the mouse is hovering over
      for (auto &entity : entities) {
        const auto &transform = entity.GetComponent<TransformComponent>();
        const auto &sprite = entity.GetComponent<SpriteComponent>();

        if (mMousePosX >= transform.position.x &&
            mMousePosX <=
                transform.position.x + sprite.width * transform.scale.x &&
            mMousePosY >= transform.position.y &&
            mMousePosY <=
                transform.position.y + sprite.height * transform.scale.y &&
            mSpriteComponent.zIndex == sprite.zIndex) {
          const auto &sprite = entity.GetComponent<SpriteComponent>();
          auto boxComponent = BoxColliderComponent();
          auto animComponent = AnimationComponent();

          if (entity.HasComponent<BoxColliderComponent>())
            mRemovedBoxComponent = entity.GetComponent<BoxColliderComponent>();
          else
            mRemovedBoxComponent = boxComponent;

          if (entity.HasComponent<AnimationComponent>())
            mRemovedAnimationComponent =
                entity.GetComponent<AnimationComponent>();
          else
            mRemovedAnimationComponent = animComponent;

          mRemovedSpriteComponent = sprite;
          mRemovedTransformComponent = transform;

          entity.Kill();
          mRightPressed = true;
          mTileRemoved = true;
          mPrevMousePosErase = pos;
          logger.Log("Tile with ID: " + std::to_string(entity.GetId()) +
                     " has been removed!");
        }
      }
      // Even if no tile was found under the cursor, advance the erase position
      // so dragging over empty cells doesn't stall the position tracker
      mPrevMousePosErase = pos;
    }
  }
}

void MouseControl::CreateCollider(const AssetManager_Ptr &assetManager,
                                  Renderer &renderer, SDL_Rect &mouseBox,
                                  SDL_Rect &camera, SDL_Event &event) {
  // Refresh mouse position for the current frame (same fix as CreateTile).
  {
    int rawX, rawY;
    SDL_GetMouseState(&rawX, &rawY);
    if (mZoom != 0.0f) {
      mMousePosX = static_cast<int>((rawX + camera.x) / mZoom);
      mMousePosY = static_cast<int>((rawY + camera.y) / mZoom);
      mMousePosScreen.x = mMousePosX;
      mMousePosScreen.y = mMousePosY;
    }
  }

  // Draw the collider mouse box
  MouseBox(assetManager, renderer, mouseBox, camera, true);

  // Do not create colliders outside of the mouse bounds
  if (MouseOutOfBounds())
    return;

  // Set the transform position to the current mousebox position
  mTransformComponent.position =
      glm::vec2(mouseBox.x + camera.x, mouseBox.y + camera.y);

  // Reset the mouse press if not pressed
  if (!LeftButtonDown())
    mLeftPressed = false;
  if (!RightButtonDown())
    mRightPressed = false;

  if (event.type == SDL_MOUSEBUTTONDOWN && !mLeftPressed) {
    if (event.button.button == SDL_BUTTON_LEFT && !mOverImGuiWindow) {
      Entity boxCollider = Registry::Instance().CreateEntity();
      boxCollider.Group("colliders");
      boxCollider.AddComponent<TransformComponent>(
          mTransformComponent.position / glm::vec2(mZoom, mZoom),
          mTransformComponent.scale, mTransformComponent.rotation);

      boxCollider.AddComponent<BoxColliderComponent>(
          mBoxColliderComponent.width, mBoxColliderComponent.height,
          mBoxColliderComponent.offset);
      mLeftPressed = true;
    }

    if (event.button.button == SDL_BUTTON_RIGHT && !mOverImGuiWindow) {
      glm::vec2 subtract =
          glm::vec2((mMouseRect.x * mTransformComponent.scale.x) / 2,
                    (mMouseRect.y * mTransformComponent.scale.y) / 2);

      // Get all the entities from the group "tiles"
      auto entities = Registry::Instance().GetEntitiesByGroup("colliders");

      // Loop through tiles and remove the one that the mouse is hovering over
      for (auto &entity : entities) {
        if (entity.HasComponent<SpriteComponent>())
          continue;

        auto &transform = entity.GetComponent<TransformComponent>();
        const auto &box_collider = entity.GetComponent<BoxColliderComponent>();

        if (mMousePosX >= transform.position.x &&
            mMousePosX <=
                transform.position.x + box_collider.width * transform.scale.x &&
            mMousePosY >= transform.position.y &&
            mMousePosY <= transform.position.y +
                              box_collider.height * transform.scale.y) {
          entity.Kill();
          mRightPressed = true;
          logger.Log("Collider with ID: " + std::to_string(entity.GetId()) +
                     " has been removed!");
        }
      }
    }
  }
}

void MouseControl::UpdateMousePos(const SDL_Rect &camera) {
  // Get the location of the mouse from SDL
  SDL_GetMouseState(&mMousePosX, &mMousePosY);

  // Add the camera position to the mouse position
  mMousePosX += camera.x;
  mMousePosY += camera.y;

  mMousePosX /= mZoom;
  mMousePosY /= mZoom;

  // This value is used for Mouse Pos monitoring in the ImGui Main Bar
  mMousePosScreen.x = mMousePosX;
  mMousePosScreen.y = mMousePosY;
}

void MouseControl::SetSpriteProperties(const std::string &assetID,
                                       const int width, const int height,
                                       const int layer, const int srcRectX,
                                       const int srcRectY) {
  mSpriteComponent.assetId = assetID;
  mSpriteComponent.width = width;
  mSpriteComponent.height = height;
  mSpriteComponent.zIndex = layer;
  mSpriteComponent.isFixed = false;
  mSpriteComponent.flip = SDL_FLIP_NONE;
  mSpriteComponent.srcRect = {srcRectX, srcRectY, width, height};
}

void MouseControl::SetTransformScale(const int scaleX, const int scaleY) {
  mTransformComponent.scale = glm::vec2(scaleX, scaleY);
}

void MouseControl::SetBoxColliderProperties(const int width, const int height,
                                            const int offsetX,
                                            const int offsetY) {
  mBoxColliderComponent.width = width;
  mBoxColliderComponent.height = height;
  mBoxColliderComponent.offset = glm::vec2(offsetX, offsetY);
}

void MouseControl::SetAnimationProperties(const int numFrames,
                                          const int frameSpeedRate,
                                          bool vertical, bool looped,
                                          int frameOffset) {
  mAnimationComponent.numFrames = numFrames;
  mAnimationComponent.frameSpeedRate = frameSpeedRate;
  mAnimationComponent.vertical = vertical;
  mAnimationComponent.isLooped = looped;
  mAnimationComponent.frameOffset = frameOffset;
}

const bool MouseControl::MouseOutOfBounds() const {
  if (mMousePosScreen.x < 0 || mMousePosScreen.y < 0)
    return true;

  return false;
}

void MouseControl::PanCamera(SDL_Rect &camera, const float &dt,
                             const AssetManager_Ptr &assetManager,
                             Renderer &renderer) {
  if (MiddleButtonDown()) {
    // Hide the mouse cursor
    SDL_ShowCursor(0);
    SDL_Rect srcRect{0, 0, 24, 24};
    // The destination rect is around the mouse cursor area
    SDL_Rect dstRect{mMousePosX * mZoom - camera.x,
                     mMousePosY * mZoom - camera.y, 48, 48};
    // Draw the mouse hand image when using the panning function
    SDL_RenderCopyEx(renderer.get(),
                     assetManager->GetTexture("mouse_hand").get(), &srcRect,
                     &dstRect, NULL, NULL, SDL_FLIP_NONE);
    // Check the current mouse values to the last pan value and move the camera
    // accordingly
    if (mPanX != mMousePosScreen.x)
      camera.x -= (mMousePosScreen.x - mPanX) * mZoom * dt * 10;

    if (mPanY != mMousePosScreen.y)
      camera.y -= (mMousePosScreen.y - mPanY) * mZoom * dt * 10;
  } else {
    // Show the original mouse cursor
    SDL_ShowCursor(1);
    // Reset the pan values to the current mouse values
    mPanX = mMousePosScreen.x;
    mPanY = mMousePosScreen.y;
  }
}
