#pragma once

// The GameState interface plus the engine surface a small game commonly wants.
// If you do not want all of that, include <stormengine2/states/gameStateBase.h>
// instead and include what you use - it is the same interface without the
// 65,736 extra preprocessed lines.
#include "gameStateBase.h"

#include "../assetStore.h"
#include "../components/animation.h"
#include "../components/boxCollider.h"
#include "../components/rigidBody.h"
#include "../components/sprite.h"
#include "../components/transform.h"
#include "../ecs.h"
#include "../logger.h"
#include "../systems/animation.h"
#include "../systems/movement.h"
#include "../systems/render.h"
#include "../systems/renderCollider.h"
#include "../tilemapLoader.h"
