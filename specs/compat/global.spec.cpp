// The one spec in the suite with no `using namespace storm;`. That is the
// point: this file proves <stormengine2/compat/global.h> alone is enough for a
// 1.x game whose sources name every engine type unqualified. Adding the
// using-directive here would make the case pass whether the bridge exported
// anything or not.
#include "../../common/compat/global.h"
#include <igloo/igloo_alt.h>

using namespace igloo;

Describe(CompatGlobalHeaderSpec) {

  // Each case below names its types unqualified. The assertions are almost
  // beside the point -- the compile is the test.
  It(should_expose_the_ecs_unqualified) {
    Registry registry;
    Entity entity = registry.CreateEntity();
    registry.Update();

    Assert::That(registry.IsAlive(entity), Equals(true));
    Assert::That(static_cast<unsigned int>(MAX_COMPONENTS), Equals(64u));

    Signature signature;
    Assert::That(signature.none(), Equals(true));
  };

  It(should_expose_components_and_systems_unqualified) {
    Registry registry;
    registry.AddSystem<MovementSystem>();

    Entity entity = registry.CreateEntity();
    entity.AddComponent<TransformComponent>(glm::vec2(1.f, 2.f),
                                            glm::vec2(1.f, 1.f), 0.0);
    entity.AddComponent<RigidBodyComponent>(glm::vec2(0.f, 0.f));
    registry.Update();

    Assert::That(registry.HasComponent<TransformComponent>(entity),
                 Equals(true));
    Assert::That(registry.GetSystem<MovementSystem>().GetSystemEntities().size(),
                 Equals(1u));
  };

  It(should_expose_the_tile_loader_unqualified) {
    Tile tile;
    Assert::That(tile.isAnimated, Equals(false));
    Assert::That(tile.numFrames, Equals(1));

    Map map;
    map.push_back(tile);
    Assert::That(map.size(), Equals(1u));
  };

  It(should_expose_input_unqualified) {
    Keyboard keyboard;
    keyboard.BeginFrame();
    Assert::That(keyboard.IsDown(SDL_SCANCODE_SPACE), Equals(false));

    ActionBinding binding;
    binding.key = SDL_SCANCODE_SPACE;
    binding.pad = GamepadButton::A;
    binding.vpad = VPadControl::A;
    binding.touch = TouchControl::Jump;

    ActionMap actions;
    actions.Bind(0, binding);
    Assert::That(actions.Count(), Equals(1u));

    GamepadState pad;
    Assert::That(GamepadDown(pad, GamepadButton::A), Equals(false));

    VPadLayout layout = MakeVPadLayout(640.f, 480.f, VPadStyle::Snes);
    Assert::That(layout.dpadRadius > 0.f, Equals(true));

    TouchZones zones = MakeDefaultZones(640.f, 480.f);
    Assert::That(zones.jump.w > 0.f, Equals(true));
  };

  // Unscoped enums are the trap this header has to get right: pulling in the
  // type does not pull in its enumerators, since each is a name in namespace
  // storm in its own right.
  It(should_expose_unscoped_enumerators_unqualified) {
    LogType type = LOG_INFO;
    Assert::That(type == LOG_INFO, Equals(true));
    Assert::That(LOG_ERROR != LOG_WARNING, Equals(true));

    Assert::That(static_cast<int>(kNetChunkVital), Equals(1));
    Assert::That(static_cast<int>(kNetControlConnect), Equals(1));
  };

  It(should_expose_networking_constants_unqualified) {
    Assert::That(kNetMaxClientsPerIp > 0, Equals(true));
    Assert::That(kNetMaxPacketSize > 0, Equals(true));

    NetAddress address;
    Assert::That(address.port, Equals(0));
  };

  It(should_expose_logging_unqualified) {
    Logger logger;
    LogEntry entry;
    entry.type = LOG_ERROR;
    entry.message = "bridged";
    Assert::That(entry.type == LOG_ERROR, Equals(true));
  };

  // A name reached through the bridge is the same type as the namespaced one,
  // not a copy -- so a value built unqualified passes to a storm:: signature.
  It(should_bridge_to_the_same_types_not_copies) {
    Registry registry;
    storm::Registry *asNamespaced = &registry;
    Assert::That(asNamespaced == &registry, Equals(true));

    Tile tile;
    storm::Tile *tileAsNamespaced = &tile;
    Assert::That(tileAsNamespaced == &tile, Equals(true));
  };
};
