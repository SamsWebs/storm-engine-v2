#include "../../common/systems/animation.h"
#include <igloo/igloo_alt.h>

using namespace igloo;
using namespace storm;

Describe(AnimationSystemSpec){
    It(should_step_through_looped_frames){// A large elapsed time that stays
                                          // congruent modulo the frame count
                                          // for any sub-second tick drift.
                                          Registry registry;
registry.AddSystem<AnimationSystem>();

Entity entity = registry.CreateEntity();
entity.AddComponent<SpriteComponent>();
entity.AddComponent<AnimationComponent>(1000, 1, false, true);
auto &animation = entity.GetComponent<AnimationComponent>();
auto &sprite = entity.GetComponent<SpriteComponent>();
animation.startTime = SDL_GetTicks() - 1000000; // 1000 frames elapsed
sprite.width = 10;
sprite.height = 20;

registry.Update();
registry.GetSystem<AnimationSystem>().Update();

Assert::That(animation.currentFrame, Equals(0)); // 1000 % 1000
Assert::That(sprite.srcRect.x, Equals(0));
}

It(should_stop_on_the_last_frame_when_not_looped) {
  Registry registry;
  registry.AddSystem<AnimationSystem>();

  Entity entity = registry.CreateEntity();
  entity.AddComponent<SpriteComponent>();
  entity.AddComponent<AnimationComponent>(4, 1, false, false);
  auto &animation = entity.GetComponent<AnimationComponent>();
  auto &sprite = entity.GetComponent<SpriteComponent>();
  animation.startTime = SDL_GetTicks() - 100000; // long past the end
  sprite.width = 10;
  sprite.height = 20;

  registry.Update();
  registry.GetSystem<AnimationSystem>().Update();

  Assert::That(animation.currentFrame, Equals(3)); // clamped, not wrapped
  Assert::That(sprite.srcRect.x, Equals(30));
}

It(should_advance_the_sheet_vertically_when_vertical) {
  Registry registry;
  registry.AddSystem<AnimationSystem>();

  Entity entity = registry.CreateEntity();
  entity.AddComponent<SpriteComponent>();
  entity.AddComponent<AnimationComponent>(4, 1, true, false); // vertical
  auto &animation = entity.GetComponent<AnimationComponent>();
  auto &sprite = entity.GetComponent<SpriteComponent>();
  animation.startTime = SDL_GetTicks() - 100000;
  sprite.width = 10;
  sprite.height = 20;

  registry.Update();
  registry.GetSystem<AnimationSystem>().Update();

  Assert::That(animation.currentFrame, Equals(3));
  Assert::That(sprite.srcRect.y, Equals(60)); // y advances on vertical sheets
  Assert::That(sprite.srcRect.x, Equals(0));
}

It(should_honor_the_frame_offset) {
  Registry registry;
  registry.AddSystem<AnimationSystem>();

  Entity entity = registry.CreateEntity();
  entity.AddComponent<SpriteComponent>();
  entity.AddComponent<AnimationComponent>(2, 1, false, true);
  auto &animation = entity.GetComponent<AnimationComponent>();
  auto &sprite = entity.GetComponent<SpriteComponent>();
  animation.frameOffset = 5;
  animation.startTime = SDL_GetTicks(); // frame 0
  sprite.width = 10;
  sprite.height = 20;

  registry.Update();
  registry.GetSystem<AnimationSystem>().Update();

  Assert::That(animation.currentFrame, Equals(0));
  Assert::That(sprite.srcRect.x, Equals(50)); // (offset + frame) * width
}

It(should_skip_entities_without_frames) {
  Registry registry;
  registry.AddSystem<AnimationSystem>();

  Entity entity = registry.CreateEntity();
  entity.AddComponent<SpriteComponent>();
  entity.AddComponent<AnimationComponent>(0, 1, false, true); // no frames
  auto &animation = entity.GetComponent<AnimationComponent>();
  auto &sprite = entity.GetComponent<SpriteComponent>();
  animation.currentFrame = 3;
  sprite.srcRect.x = 7;

  registry.Update();
  registry.GetSystem<AnimationSystem>().Update();

  Assert::That(animation.currentFrame, Equals(3)); // untouched
  Assert::That(sprite.srcRect.x, Equals(7));
}
}
;
