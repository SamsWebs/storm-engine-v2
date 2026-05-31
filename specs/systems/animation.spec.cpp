#include "../../common/systems/animation.h"
#include <igloo/igloo_alt.h>

using namespace igloo;

Describe(AnimationSystemSpec){
    It(should_update_sprite_source_rect_based_on_animation){// Arrange
                                                            Registry registry;
AnimationSystem animationSystem;

Entity entity = registry.CreateEntity();

// Add SpriteComponent and AnimationComponent to the entity
entity.AddComponent<SpriteComponent>();
entity.AddComponent<AnimationComponent>();

// Set up the initial values for the AnimationComponent and SpriteComponent
entity.GetComponent<AnimationComponent>().numFrames = 4;
entity.GetComponent<AnimationComponent>().currentFrame = 0;
entity.GetComponent<AnimationComponent>().frameSpeedRate = 1;
entity.GetComponent<AnimationComponent>().isLooped = true;
entity.GetComponent<AnimationComponent>().startTime = 0;

entity.GetComponent<SpriteComponent>().width = 100;
entity.GetComponent<SpriteComponent>().height = 100;
entity.GetComponent<SpriteComponent>().srcRect.x = 0;

// Act
animationSystem.Update();

// Assert
Assert::That(entity.GetComponent<AnimationComponent>().currentFrame, Equals(0));
Assert::That(entity.GetComponent<SpriteComponent>().srcRect.x, Equals(0));

// Advance the simulation time by 500 milliseconds
entity.GetComponent<AnimationComponent>().startTime = 500;
animationSystem.Update();

// Assert
// Assert::That(entity.GetComponent<AnimationComponent>().currentFrame,
// Equals(2));
// Assert::That(entity.GetComponent<SpriteComponent>().srcRect.x, Equals(200));

// Advance the simulation time by 2000 milliseconds
entity.GetComponent<AnimationComponent>().startTime = 2500;
animationSystem.Update();

// Assert
Assert::That(entity.GetComponent<AnimationComponent>().currentFrame, Equals(0));
Assert::That(entity.GetComponent<SpriteComponent>().srcRect.x, Equals(0));
}

It(should_not_update_when_no_entities_exist) {
  // Arrange
  Registry registry;
  AnimationSystem animationSystem;

  // Act
  animationSystem.Update();

  // Assert
  // Add your assertions here based on the expected behavior of the animation
  // system For example, you can check if the AnimationComponent and
  // SpriteComponent of existing entities were not modified
}
}
;
