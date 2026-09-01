#pragma once

// ── Compatibility bridge: the engine's names in the global namespace ────────
//
// 2.0.0 moved every engine type into `namespace storm`. Include this header and
// a game written against 1.x keeps compiling unchanged:
//
//     #include <stormengine2/compat/global.h>
//
// For a game whose engine includes are spread across many files, the cheapest
// migration is a force-include from the build rather than editing each one:
//
//     CXXFLAGS += -include stormengine2/compat/global.h
//
// **This header exists to be deleted.** It is a bridge, not an API. Every name
// it emits is a `using` declaration pulling `storm::X` into the global
// namespace - which is precisely the pollution the namespace was added to stop,
// so a game that keeps this include forever has taken none of the benefit. The
// migration is: add the include, confirm the build is green, then remove it and
// fix the names, either with `using namespace storm;` in your own files or by
// qualifying. A future major will drop this header.
//
// It includes the whole engine by design - a compatibility shim cannot know
// which parts a game uses. Reach for the individual headers in your own code.

#include "../assetStore.h"
#include "../collision/shapes.h"
#include "../ecs.h"
#include "../gameStateMachine.h"
#include "../logger.h"
#include "../text.h"
#include "../tilemapLoader.h"
#include "../xmlLoader.h"

#include "../components/animation.h"
#include "../components/boxCollider.h"
#include "../components/rigidBody.h"
#include "../components/sprite.h"
#include "../components/transform.h"

#include "../input/actionMap.h"
#include "../input/gamepad.h"
#include "../input/keyboard.h"
#include "../input/touchControls.h"
#include "../input/virtualGamepad.h"

#include "../net/net.h"
#include "../net/netClient.h"
#include "../net/netConnection.h"
#include "../net/netPacket.h"
#include "../net/netServer.h"
#include "../net/netSnapshot.h"
#include "../net/netSocket.h"
#include "../net/netTypes.h"
#include "../net/netVarInt.h"

#include "../states/gameStateBase.h"

#include "../systems/animation.h"
#include "../systems/contact.h"
#include "../systems/movement.h"
#include "../systems/render.h"
#include "../systems/renderCollider.h"

// ── ECS ─────────────────────────────────────────────────────────────────────
using storm::Component;
using storm::ComponentMiss;
using storm::ECS_MAX_DIAGNOSTIC_REPORTS;
using storm::ComponentMissDescription;
using storm::EcsComponentIdIsValid;
using storm::EcsFallbackComponent;
using storm::EcsReportErr;
using storm::EcsShouldReport;
using storm::EcsSuppressionNote;
using storm::Entity;
using storm::EntityOrder;
using storm::IComponent;
using storm::IPool;
using storm::MAX_COMPONENTS;
using storm::Pool;
using storm::Registry;
using storm::Signature;
using storm::System;

// ── Assets, text, loaders ───────────────────────────────────────────────────
using storm::AssetStore;
using storm::AssetStore_Ptr;
using storm::LoadTexturesFromXml;
using storm::Map;
using storm::Text;
using storm::Tile;
using storm::TileMapLoader;
using storm::XmlLoader;
using storm::XmlObjectDef;
using storm::XmlTextureDef;

// ── Logging ─────────────────────────────────────────────────────────────────
// LogType is an unscoped enum, so its enumerators are members of namespace
// storm in their own right: pulling in the type does not pull in the names.
using storm::LogEntry;
using storm::Logger;
using storm::Logger_Ptr;
using storm::LogType;
using storm::LOG_ERROR;
using storm::LOG_INFO;
using storm::LOG_WARNING;

// ── Components ──────────────────────────────────────────────────────────────
using storm::AnimationComponent;
using storm::BoxColliderComponent;
using storm::RigidBodyComponent;
using storm::SpriteComponent;
using storm::TransformComponent;

// ── Collision math ──────────────────────────────────────────────────────────
// Overlaps, Manifold and MinimumTranslation are overloaded free functions; one
// using-declaration each brings every overload across.
using storm::ClosestPointOn;
using storm::ContactCircle;
using storm::Manifold;
using storm::MinimumTranslation;
using storm::Overlaps;

// ── Systems ─────────────────────────────────────────────────────────────────
using storm::AnimationSystem;
using storm::Contact;
using storm::ContactAABB;
using storm::ContactSystem;
using storm::MovementSystem;
using storm::RenderColliderSystem;
using storm::RenderSystem;

// ── State machine ───────────────────────────────────────────────────────────
using storm::FPS;
using storm::GameState;
using storm::GameStateMachine;
using storm::MILLISECS_PER_FRAME;

// ── Input ───────────────────────────────────────────────────────────────────
using storm::ActionBinding;
using storm::ActionMap;
using storm::ActionSources;
using storm::DpadFromPoint;
using storm::EvalTouches;
using storm::EvalVPad;
using storm::Gamepad;
using storm::GamepadButton;
using storm::GamepadDown;
using storm::GamepadNormaliseStick;
using storm::GamepadPressed;
using storm::GamepadReleased;
using storm::GamepadState;
using storm::Keyboard;
using storm::MakeDefaultZones;
using storm::MakeVPadLayout;
using storm::TouchControl;
using storm::TouchInput;
using storm::TouchPoint;
using storm::TouchZone;
using storm::TouchZones;
using storm::VPadControl;
using storm::VPadLayout;
using storm::VPadState;
using storm::VPadStyle;

// ── Networking ──────────────────────────────────────────────────────────────
using storm::NetAddress;
using storm::NetAddressFromParts;
using storm::NetAddressToString;
using storm::NetChunk;
using storm::NetChunkHeader;
using storm::NetClient;
using storm::NetConnection;
using storm::NetControlMessage;
using storm::NetControlPacket;
using storm::NetIpToHost;
using storm::NetMessageReader;
using storm::NetMessageWriter;
using storm::NetNonce32;
using storm::NetNowMs;
using storm::NetPacketHeaderPack;
using storm::NetPacketHeaderUnpack;
using storm::NetPortToHost;
using storm::NetRandom32;
using storm::NetResolveAddress;
using storm::NetSendControl;
using storm::NetServer;
using storm::NetSnapshot;
using storm::NetSnapshotCache;
using storm::NetSnapshotDelta;
using storm::NetSocket;
using storm::NetVarIntPack;
using storm::NetVarIntUnpack;
using storm::NonceToToken;
using storm::TokenToNonce;

// Unscoped net enums: the type and its enumerators are separate names.
using storm::NetChunkFlag;
using storm::kNetChunkResend;
using storm::kNetChunkVital;
using storm::NetPacketFlag;
using storm::kNetPacketResend;
using storm::kNetControlAccept;
using storm::kNetControlClose;
using storm::kNetControlConnect;
using storm::kNetControlConnectAccept;
using storm::kNetControlConnectReady;

// Net constants.
using storm::kNetControlMagic;
using storm::kNetFlushMs;
using storm::kNetHandshakeRetryMs;
using storm::kNetHardResendMs;
using storm::kNetKeepaliveMs;
using storm::kNetMaxChunkSize;
using storm::kNetMaxChunksPerPacket;
using storm::kNetMaxClients;
using storm::kNetMaxClientsPerIp;
using storm::kNetMaxPacketSize;
using storm::kNetMaxPayload;
using storm::kNetMaxSequence;
using storm::kNetResendBufferSize;
using storm::kNetResendMaxEntries;
using storm::kNetResendMs;
using storm::kNetSequenceBits;
using storm::kNetTimeoutMs;
using storm::kNetVarIntMaxBytes;
