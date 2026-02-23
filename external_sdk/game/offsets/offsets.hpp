#pragma once

#include <cstdint>
#include <string>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <functional>

#ifdef _WIN32
#include <urlmon.h>
#pragma comment(lib, "urlmon.lib")
#endif

namespace offsets {
    inline std::string ClientVersion = "version-5b077c09380f4fe6";

    namespace AnimationTrack {
        inline uintptr_t Animation = 0xd0;
        inline uintptr_t Animator = 0x118;
        inline uintptr_t IsPlaying = 0x2bd;
        inline uintptr_t Looped = 0xf5;
        inline uintptr_t Speed = 0xe4;
    }

    namespace BasePart {
        inline uintptr_t AssemblyAngularVelocity = 0xfc;
        inline uintptr_t AssemblyLinearVelocity = 0xf0;
        inline uintptr_t Color3 = 0x194;
        inline uintptr_t Material = 0x228;
        inline uintptr_t Position = 0x12c;
        inline uintptr_t Primitive = 0x148;
        inline uintptr_t PrimitiveFlags = 0x1ae;
        inline uintptr_t PrimitiveOwner = 0x1f0;
        inline uintptr_t Rotation = 0xc0;
        inline uintptr_t Shape = 0x1b1;
        inline uintptr_t Size = 0x1b0;
        inline uintptr_t Transparency = 0xf0;
        inline uintptr_t ValidatePrimitive = 0x6;
    }

    namespace ByteCode {
        inline uintptr_t Pointer = 0x10;
        inline uintptr_t Size = 0x20;
    }

    namespace Camera {
        inline uintptr_t CameraSubject = 0xe8;
        inline uintptr_t CameraType = 0x158;
        inline uintptr_t FieldOfView = 0x160;
        inline uintptr_t Position = 0x11c;
        inline uintptr_t Rotation = 0xf8;
    }

    namespace ClickDetector {
        inline uintptr_t MaxActivationDistance = 0x100;
        inline uintptr_t MouseIcon = 0xe0;
    }

    namespace DataModel {
        inline uintptr_t CreatorId = 0x188;
        inline uintptr_t GameId = 0x190;
        inline uintptr_t GameLoaded = 0x600;
        inline uintptr_t JobId = 0x138;
        inline uintptr_t PlaceId = 0x198;
        inline uintptr_t PlaceVersion = 0x1b4;
        inline uintptr_t PrimitiveCount = 0x440;
        inline uintptr_t ScriptContext = 0x3f0;
        inline uintptr_t ServerIP = 0x5e8;
        inline uintptr_t Workspace = 0x178;
    }

    namespace FFlags {
        inline uintptr_t DebugDisableTimeoutDisconnect = 0x67ae7a0;
        inline uintptr_t EnableLoadModule = 0x679d8f8;
        inline uintptr_t PartyPlayerInactivityTimeoutInSeconds = 0x676ca70;
        inline uintptr_t TaskSchedulerTargetFps = 0x74c0e6c;
        inline uintptr_t WebSocketServiceEnableClientCreation = 0x67bb778;
    }

    namespace FakeDataModel {
        inline uintptr_t Pointer = 0x7d03628;
        inline uintptr_t RealDataModel = 0x1c0;
    }

    namespace GuiObject {
        inline uintptr_t BackgroundColor3 = 0x550;
        inline uintptr_t BorderColor3 = 0x55c;
        inline uintptr_t Image = 0xa48;
        inline uintptr_t LayoutOrder = 0x58c;
        inline uintptr_t Position = 0x520;
        inline uintptr_t RichText = 0xae0;
        inline uintptr_t Rotation = 0x188;
        inline uintptr_t ScreenGui_Enabled = 0x4d4;
        inline uintptr_t Size = 0x540;
        inline uintptr_t Text = 0xe40;
        inline uintptr_t TextColor3 = 0xef0;
        inline uintptr_t Visible = 0x5b9;
    }

    namespace Humanoid {
        inline uintptr_t AutoRotate = 0x1d9;
        inline uintptr_t FloorMaterial = 0x190;
        inline uintptr_t Health = 0x194;
        inline uintptr_t HipHeight = 0x1a0;
        inline uintptr_t HumanoidState = 0x8d8;
        inline uintptr_t HumanoidStateID = 0x20;
        inline uintptr_t Jump = 0x1dd;
        inline uintptr_t JumpHeight = 0x1ac;
        inline uintptr_t JumpPower = 0x1b0;
        inline uintptr_t MaxHealth = 0x1b4;
        inline uintptr_t MaxSlopeAngle = 0x1b8;
        inline uintptr_t MoveDirection = 0x158;
        inline uintptr_t RigType = 0x1c8;
        inline uintptr_t Walkspeed = 0x1d4;
        inline uintptr_t WalkspeedCheck = 0x3c0;
    }

    namespace Instance {
        inline uintptr_t AttributeContainer = 0x48;
        inline uintptr_t AttributeList = 0x18;
        inline uintptr_t AttributeToNext = 0x58;
        inline uintptr_t AttributeToValue = 0x18;
        inline uintptr_t ChildrenEnd = 0x8;
        inline uintptr_t ChildrenStart = 0x70;
        inline uintptr_t ClassBase = 0xc58;
        inline uintptr_t ClassDescriptor = 0x18;
        inline uintptr_t ClassName = 0x8;
        inline uintptr_t Name = 0xb0;
        inline uintptr_t Parent = 0x68;
    }

    namespace Lighting {
        inline uintptr_t Ambient = 0xd8;
        inline uintptr_t Brightness = 0x120;
        inline uintptr_t ClockTime = 0x1b8;
        inline uintptr_t ColorShift_Bottom = 0xf0;
        inline uintptr_t ColorShift_Top = 0xe4;
        inline uintptr_t ExposureCompensation = 0x12c;
        inline uintptr_t FogColor = 0xfc;
        inline uintptr_t FogEnd = 0x134;
        inline uintptr_t FogStart = 0x138;
        inline uintptr_t GeographicLatitude = 0x190;
        inline uintptr_t OutdoorAmbient = 0x108;
    }

    namespace LocalScript {
        inline uintptr_t ByteCode = 0x1a8;
    }

    namespace MeshPart {
        inline uintptr_t MeshId = 0x2e0;
        inline uintptr_t Texture = 0x310;
    }

    namespace Misc {
        inline uintptr_t Adornee = 0x108;
        inline uintptr_t AnimationId = 0xd0;
        inline uintptr_t RequireLock = 0x0;
        inline uintptr_t StringLength = 0x10;
        inline uintptr_t Value = 0xd0;
    }

    namespace Model {
        inline uintptr_t PrimaryPart = 0x268;
        inline uintptr_t Scale = 0x164;
    }

    namespace ModuleScript {
        inline uintptr_t ByteCode = 0x150;
    }

    namespace MouseService {
        inline uintptr_t InputObject = 0x0;
        inline uintptr_t MousePosition = 0x0;
        inline uintptr_t SensitivityPointer = 0x7daf110;
    }

    namespace Player {
        inline uintptr_t CameraMode = 0x2f8;
        inline uintptr_t Country = 0x110;
        inline uintptr_t DisplayName = 0x130;
        inline uintptr_t Gender = 0xe68;
        inline uintptr_t LocalPlayer = 0x130;
        inline uintptr_t MaxZoomDistance = 0x2f0;
        inline uintptr_t MinZoomDistance = 0x2f4;
        inline uintptr_t ModelInstance = 0x360;
        inline uintptr_t Mouse = 0xcd8;
        inline uintptr_t Team = 0x270;
        inline uintptr_t UserId = 0x298;
    }

    namespace PlayerConfigurer {
        inline uintptr_t OverrideDuration = 0x5894805;
        inline uintptr_t Pointer = 0x7ce11e8;
    }

    namespace PlayerMouse {
        inline uintptr_t Icon = 0xe0;
        inline uintptr_t Workspace = 0x168;
    }

    namespace PrimitiveFlags {
        inline uintptr_t Anchored = 0x2;
        inline uintptr_t CanCollide = 0x8;
        inline uintptr_t CanTouch = 0x10;
    }

    namespace ProximityPrompt {
        inline uintptr_t ActionText = 0xd0;
        inline uintptr_t Enabled = 0x156;
        inline uintptr_t GamepadKeyCode = 0x13c;
        inline uintptr_t HoldDuration = 0x140;
        inline uintptr_t KeyCode = 0x9f;
        inline uintptr_t MaxActivationDistance = 0x148;
        inline uintptr_t ObjectText = 0xf0;
        inline uintptr_t RequiresLineOfSight = 0x157;
    }

    namespace RenderView {
        inline uintptr_t DeviceD3D11 = 0x8;
        inline uintptr_t VisualEngine = 0x10;
    }

    namespace RunService {
        inline uintptr_t HeartbeatFPS = 0xb8;
        inline uintptr_t HeartbeatTask = 0xe8;
    }

    namespace Sky {
        inline uintptr_t MoonAngularSize = 0x25c;
        inline uintptr_t MoonTextureId = 0xe0;
        inline uintptr_t SkyboxBk = 0x110;
        inline uintptr_t SkyboxDn = 0x140;
        inline uintptr_t SkyboxFt = 0x170;
        inline uintptr_t SkyboxLf = 0x1a0;
        inline uintptr_t SkyboxOrientation = 0x250;
        inline uintptr_t SkyboxRt = 0x1d0;
        inline uintptr_t SkyboxUp = 0x200;
        inline uintptr_t StarCount = 0x260;
        inline uintptr_t SunAngularSize = 0x254;
        inline uintptr_t SunTextureId = 0x230;
    }

    namespace SpecialMesh {
        inline uintptr_t MeshId = 0x108;
        inline uintptr_t Scale = 0x164;
    }

    namespace StatsItem {
        inline uintptr_t Value = 0x1c8;
    }

    namespace TaskScheduler {
        inline uintptr_t FakeDataModelToDataModel = 0x1b0;
        inline uintptr_t JobEnd = 0x1d8;
        inline uintptr_t JobName = 0x18;
        inline uintptr_t JobStart = 0x1d0;
        inline uintptr_t MaxFPS = 0x1b0;
        inline uintptr_t Pointer = 0x7e1cb88;
        inline uintptr_t RenderJobToFakeDataModel = 0x38;
        inline uintptr_t RenderJobToRenderView = 0x218;
    }

    namespace Team {
        inline uintptr_t BrickColor = 0xd0;
    }

    namespace Textures {
        inline uintptr_t Decal_Texture = 0x198;
        inline uintptr_t Texture_Texture = 0x198;
    }

    namespace VisualEngine {
        inline uintptr_t Dimensions = 0x720;
        inline uintptr_t Pointer = 0x7a69470;
        inline uintptr_t ToDataModel1 = 0x700;
        inline uintptr_t ToDataModel2 = 0x1c0;
        inline uintptr_t ViewMatrix = 0x4b0;
    }

    namespace Workspace {
        inline uintptr_t CurrentCamera = 0x450;
        inline uintptr_t DistributedGameTime = 0x470;
        inline uintptr_t Gravity = 0x1d0;
        inline uintptr_t GravityContainer = 0x3c8;
        inline uintptr_t PrimitivesPointer1 = 0x3c8;
        inline uintptr_t PrimitivesPointer2 = 0x240;
        inline uintptr_t ReadOnlyGravity = 0x9b0;
    }

    // Aliases
    inline uintptr_t& Animation = AnimationTrack::Animation;
    inline uintptr_t& Animator = AnimationTrack::Animator;
    inline uintptr_t& IsPlaying = AnimationTrack::IsPlaying;
    inline uintptr_t& Looped = AnimationTrack::Looped;
    inline uintptr_t& Speed = AnimationTrack::Speed;
    inline uintptr_t& AssemblyAngularVelocity = BasePart::AssemblyAngularVelocity;
    inline uintptr_t& AssemblyLinearVelocity = BasePart::AssemblyLinearVelocity;
    inline uintptr_t& Color3 = BasePart::Color3;
    inline uintptr_t& Material = BasePart::Material;
    inline uintptr_t& MaterialType = BasePart::Material;
    inline uintptr_t& Position = BasePart::Position;
    inline uintptr_t& Primitive = BasePart::Primitive;
    inline uintptr_t& PrimitiveOwner = BasePart::PrimitiveOwner;
    inline uintptr_t& Rotation = BasePart::Rotation;
    inline uintptr_t& CFrame = BasePart::Rotation;
    inline uintptr_t& Shape = BasePart::Shape;
    inline uintptr_t& Size = BasePart::Size;
    inline uintptr_t& PartSize = BasePart::Size;
    inline uintptr_t& Transparency = BasePart::Transparency;
    inline uintptr_t& ValidatePrimitive = BasePart::ValidatePrimitive;
    inline uintptr_t& PrimitiveValidateValue = BasePart::ValidatePrimitive;
    inline uintptr_t& Velocity = BasePart::AssemblyLinearVelocity;
    inline uintptr_t& ByteCodePointer = ByteCode::Pointer;
    inline uintptr_t& LocalScriptBytecodePointer = ByteCode::Pointer;
    inline uintptr_t& ModuleScriptBytecodePointer = ByteCode::Pointer;
    inline uintptr_t& ByteCodeSize = ByteCode::Size;
    inline uintptr_t& CameraSubject = Camera::CameraSubject;
    inline uintptr_t& CameraType = Camera::CameraType;
    inline uintptr_t& FieldOfView = Camera::FieldOfView;
    inline uintptr_t& FOV = Camera::FieldOfView;
    inline uintptr_t& CameraPos = Camera::Position;
    inline uintptr_t& CameraRotation = Camera::Rotation;
    inline uintptr_t& ClickDetectorMaxActivationDistance = ClickDetector::MaxActivationDistance;
    inline uintptr_t& MouseIcon = ClickDetector::MouseIcon;
    inline uintptr_t& CreatorId = DataModel::CreatorId;
    inline uintptr_t& GameId = DataModel::GameId;
    inline uintptr_t& GameLoaded = DataModel::GameLoaded;
    inline uintptr_t& JobId = DataModel::JobId;
    inline uintptr_t& PlaceId = DataModel::PlaceId;
    inline uintptr_t& PlaceVersion = DataModel::PlaceVersion;
    inline uintptr_t& PrimitiveCount = DataModel::PrimitiveCount;
    inline uintptr_t& DataModelPrimitiveCount = DataModel::PrimitiveCount;
    inline uintptr_t& ScriptContext = DataModel::ScriptContext;
    inline uintptr_t& ServerIP = DataModel::ServerIP;
    inline uintptr_t& DebugDisableTimeoutDisconnect = FFlags::DebugDisableTimeoutDisconnect;
    inline uintptr_t& EnableLoadModule = FFlags::EnableLoadModule;
    inline uintptr_t& PartyPlayerInactivityTimeoutInSeconds = FFlags::PartyPlayerInactivityTimeoutInSeconds;
    inline uintptr_t& TaskSchedulerTargetFps = FFlags::TaskSchedulerTargetFps;
    inline uintptr_t& WebSocketServiceEnableClientCreation = FFlags::WebSocketServiceEnableClientCreation;
    inline uintptr_t& FFlagList = FFlags::DebugDisableTimeoutDisconnect;
    inline uintptr_t& FakeDataModelPointer = FakeDataModel::Pointer;
    inline uintptr_t& FakeDataModelToDataModel = FakeDataModel::RealDataModel;
    inline uintptr_t& BackgroundColor3 = GuiObject::BackgroundColor3;
    inline uintptr_t& BorderColor3 = GuiObject::BorderColor3;
    inline uintptr_t& Image = GuiObject::Image;
    inline uintptr_t& LayoutOrder = GuiObject::LayoutOrder;
    inline uintptr_t& FramePositionX = GuiObject::Position;
    inline uintptr_t& FramePositionY = GuiObject::Position;
    inline uintptr_t& RichText = GuiObject::RichText;
    inline uintptr_t& FrameRotation = GuiObject::Rotation;
    inline uintptr_t& ScreenGuiEnabled = GuiObject::ScreenGui_Enabled;
    inline uintptr_t& FrameSizeX = GuiObject::Size;
    inline uintptr_t& FrameSizeY = GuiObject::Size;
    inline uintptr_t& Text = GuiObject::Text;
    inline uintptr_t& TextLabelText = GuiObject::Text;
    inline uintptr_t& TextColor3 = GuiObject::TextColor3;
    inline uintptr_t& Visible = GuiObject::Visible;
    inline uintptr_t& TextLabelVisible = GuiObject::Visible;
    inline uintptr_t& FrameVisible = GuiObject::Visible;
    inline uintptr_t& AutoRotate = Humanoid::AutoRotate;
    inline uintptr_t& FloorMaterial = Humanoid::FloorMaterial;
    inline uintptr_t& Health = Humanoid::Health;
    inline uintptr_t& HipHeight = Humanoid::HipHeight;
    inline uintptr_t& HumanoidState = Humanoid::HumanoidState;
    inline uintptr_t& HumanoidStateID = Humanoid::HumanoidStateID;
    inline uintptr_t& HumanoidStateId = Humanoid::HumanoidStateID;
    inline uintptr_t& Jump = Humanoid::Jump;
    inline uintptr_t& EvaluateStateMachine = Humanoid::Jump;
    inline uintptr_t& Sit = Humanoid::Jump;
    inline uintptr_t& JumpHeight = Humanoid::JumpHeight;
    inline uintptr_t& JumpPower = Humanoid::JumpPower;
    inline uintptr_t& MaxHealth = Humanoid::MaxHealth;
    inline uintptr_t& MaxSlopeAngle = Humanoid::MaxSlopeAngle;
    inline uintptr_t& MoveDirection = Humanoid::MoveDirection;
    inline uintptr_t& RigType = Humanoid::RigType;
    inline uintptr_t& Walkspeed = Humanoid::Walkspeed;
    inline uintptr_t& WalkSpeed = Humanoid::Walkspeed;
    inline uintptr_t& WalkspeedCheck = Humanoid::WalkspeedCheck;
    inline uintptr_t& WalkSpeedCheck = Humanoid::WalkspeedCheck;
    inline uintptr_t& AttributeContainer = Instance::AttributeContainer;
    inline uintptr_t& InstanceAttributePointer1 = Instance::AttributeContainer;
    inline uintptr_t& AttributeList = Instance::AttributeList;
    inline uintptr_t& InstanceAttributePointer2 = Instance::AttributeList;
    inline uintptr_t& AttributeToNext = Instance::AttributeToNext;
    inline uintptr_t& AttributeToValue = Instance::AttributeToValue;
    inline uintptr_t& ChildrenEnd = Instance::ChildrenEnd;
    inline uintptr_t& ChildrenStart = Instance::ChildrenStart;
    inline uintptr_t& Children = Instance::ChildrenStart;
    inline uintptr_t& ClassBase = Instance::ClassBase;
    inline uintptr_t& ClassDescriptor = Instance::ClassDescriptor;
    inline uintptr_t& ClassName = Instance::ClassName;
    inline uintptr_t& ClassDescriptorToClassName = Instance::ClassName;
    inline uintptr_t& Name = Instance::Name;
    inline uintptr_t& NameSize = Instance::Name;
    inline uintptr_t& Parent = Instance::Parent;
    inline uintptr_t& Ambient = Lighting::Ambient;
    inline uintptr_t& Brightness = Lighting::Brightness;
    inline uintptr_t& ClockTime = Lighting::ClockTime;
    inline uintptr_t& ColorShift_Bottom = Lighting::ColorShift_Bottom;
    inline uintptr_t& ColorShift_Top = Lighting::ColorShift_Top;
    inline uintptr_t& ExposureCompensation = Lighting::ExposureCompensation;
    inline uintptr_t& FogColor = Lighting::FogColor;
    inline uintptr_t& FogEnd = Lighting::FogEnd;
    inline uintptr_t& FogStart = Lighting::FogStart;
    inline uintptr_t& GeographicLatitude = Lighting::GeographicLatitude;
    inline uintptr_t& OutdoorAmbient = Lighting::OutdoorAmbient;
    inline uintptr_t& LocalScriptByteCode = LocalScript::ByteCode;
    inline uintptr_t& MeshId = MeshPart::MeshId;
    inline uintptr_t& MeshPartTexture = MeshPart::Texture;
    inline uintptr_t& Texture = MeshPart::Texture;
    inline uintptr_t& Adornee = Misc::Adornee;
    inline uintptr_t& AnimationId = Misc::AnimationId;
    inline uintptr_t& RequireLock = Misc::RequireLock;
    inline uintptr_t& RequireBypass = Misc::RequireLock;
    inline uintptr_t& StringLength = Misc::StringLength;
    inline uintptr_t& Value = Misc::Value;
    inline uintptr_t& PrimaryPart = Model::PrimaryPart;
    inline uintptr_t& Scale = Model::Scale;
    inline uintptr_t& ModuleScriptByteCode = ModuleScript::ByteCode;
    inline uintptr_t& InputObject = MouseService::InputObject;
    inline uintptr_t& MousePosition = MouseService::MousePosition;
    inline uintptr_t& SensitivityPointer = MouseService::SensitivityPointer;
    inline uintptr_t& MouseSensitivity = MouseService::SensitivityPointer;
    inline uintptr_t& CameraMode = Player::CameraMode;
    inline uintptr_t& Country = Player::Country;
    inline uintptr_t& DisplayName = Player::DisplayName;
    inline uintptr_t& HumanoidDisplayName = Player::DisplayName;
    inline uintptr_t& Gender = Player::Gender;
    inline uintptr_t& LocalPlayer = Player::LocalPlayer;
    inline uintptr_t& MaxZoomDistance = Player::MaxZoomDistance;
    inline uintptr_t& CameraMaxZoomDistance = Player::MaxZoomDistance;
    inline uintptr_t& MinZoomDistance = Player::MinZoomDistance;
    inline uintptr_t& CameraMinZoomDistance = Player::MinZoomDistance;
    inline uintptr_t& ModelInstance = Player::ModelInstance;
    inline uintptr_t& Mouse = Player::Mouse;
    inline uintptr_t& UserId = Player::UserId;
    inline uintptr_t& CharacterAppearanceId = Player::UserId;
    inline uintptr_t& OverrideDuration = PlayerConfigurer::OverrideDuration;
    inline uintptr_t& PlayerConfigurerPointer = PlayerConfigurer::Pointer;
    inline uintptr_t& Icon = PlayerMouse::Icon;
    inline uintptr_t& WorkspacePtr = PlayerMouse::Workspace;
    inline uintptr_t& Anchored = PrimitiveFlags::Anchored;
    inline uintptr_t& AnchoredMask = PrimitiveFlags::Anchored;
    inline uintptr_t& CanCollide = PrimitiveFlags::CanCollide;
    inline uintptr_t& CanCollideMask = PrimitiveFlags::CanCollide;
    inline uintptr_t& CanTouch = PrimitiveFlags::CanTouch;
    inline uintptr_t& CanTouchMask = PrimitiveFlags::CanTouch;
    inline uintptr_t& ProximityPromptActionText = ProximityPrompt::ActionText;
    inline uintptr_t& ProximityPromptEnabled = ProximityPrompt::Enabled;
    inline uintptr_t& ProximityPromptGamepadKeyCode = ProximityPrompt::GamepadKeyCode;
    inline uintptr_t& ProximityPromptHoldDuration = ProximityPrompt::HoldDuration;
    inline uintptr_t& ProximityPromptHoldDuraction = ProximityPrompt::HoldDuration;
    inline uintptr_t& KeyCode = ProximityPrompt::KeyCode;
    inline uintptr_t& ProximityPromptMaxActivationDistance = ProximityPrompt::MaxActivationDistance;
    inline uintptr_t& ProximityPromptMaxObjectText = ProximityPrompt::ObjectText;
    inline uintptr_t& RequiresLineOfSight = ProximityPrompt::RequiresLineOfSight;
    inline uintptr_t& DeviceD3D11 = RenderView::DeviceD3D11;
    inline uintptr_t& HeartbeatFPS = RunService::HeartbeatFPS;
    inline uintptr_t& HeartbeatTask = RunService::HeartbeatTask;
    inline uintptr_t& MoonAngularSize = Sky::MoonAngularSize;
    inline uintptr_t& MoonTextureId = Sky::MoonTextureId;
    inline uintptr_t& SkyboxBk = Sky::SkyboxBk;
    inline uintptr_t& SkyboxDn = Sky::SkyboxDn;
    inline uintptr_t& SkyboxFt = Sky::SkyboxFt;
    inline uintptr_t& SkyboxLf = Sky::SkyboxLf;
    inline uintptr_t& SkyboxOrientation = Sky::SkyboxOrientation;
    inline uintptr_t& SkyboxRt = Sky::SkyboxRt;
    inline uintptr_t& SkyboxUp = Sky::SkyboxUp;
    inline uintptr_t& StarCount = Sky::StarCount;
    inline uintptr_t& SunAngularSize = Sky::SunAngularSize;
    inline uintptr_t& SunTextureId = Sky::SunTextureId;
    inline uintptr_t& SpecialMeshId = SpecialMesh::MeshId;
    inline uintptr_t& SpecialMeshScale = SpecialMesh::Scale;
    inline uintptr_t& StatsValue = StatsItem::Value;
    inline uintptr_t& FakeDataModelToDataModelTask = TaskScheduler::FakeDataModelToDataModel;
    inline uintptr_t& JobEnd = TaskScheduler::JobEnd;
    inline uintptr_t& JobName = TaskScheduler::JobName;
    inline uintptr_t& Job_Name = TaskScheduler::JobName;
    inline uintptr_t& JobStart = TaskScheduler::JobStart;
    inline uintptr_t& MaxFPS = TaskScheduler::MaxFPS;
    inline uintptr_t& TaskSchedulerMaxFPS = TaskScheduler::MaxFPS;
    inline uintptr_t& TaskSchedulerPointer = TaskScheduler::Pointer;
    inline uintptr_t& JobsPointer = TaskScheduler::Pointer;
    inline uintptr_t& RenderJobToFakeDataModel = TaskScheduler::RenderJobToFakeDataModel;
    inline uintptr_t& RenderJobToRenderView = TaskScheduler::RenderJobToRenderView;
    inline uintptr_t& BrickColor = Team::BrickColor;
    inline uintptr_t& TeamColor = Team::BrickColor;
    inline uintptr_t& DecalTexture = Textures::Decal_Texture;
    inline uintptr_t& TextureTexture = Textures::Texture_Texture;
    inline uintptr_t& SoundId = Textures::Decal_Texture;
    inline uintptr_t& Dimensions = VisualEngine::Dimensions;
    inline uintptr_t& VisualEnginePointer = VisualEngine::Pointer;
    inline uintptr_t& ToDataModel1 = VisualEngine::ToDataModel1;
    inline uintptr_t& VisualEngineToDataModel1 = VisualEngine::ToDataModel1;
    inline uintptr_t& ToDataModel2 = VisualEngine::ToDataModel2;
    inline uintptr_t& VisualEngineToDataModel2 = VisualEngine::ToDataModel2;
    inline uintptr_t& ViewMatrix = VisualEngine::ViewMatrix;
    inline uintptr_t& viewmatrix = VisualEngine::ViewMatrix;
    inline uintptr_t& CurrentCamera = Workspace::CurrentCamera;
    inline uintptr_t& DistributedGameTime = Workspace::DistributedGameTime;
    inline uintptr_t& Gravity = Workspace::Gravity;
    inline uintptr_t& GravityContainer = Workspace::GravityContainer;
    inline uintptr_t& WorkspaceToWorld = Workspace::GravityContainer;
    inline uintptr_t& PrimitivesPointer1 = Workspace::PrimitivesPointer1;
    inline uintptr_t& PrimitivesPointer2 = Workspace::PrimitivesPointer2;
    inline uintptr_t& ReadOnlyGravity = Workspace::ReadOnlyGravity;
    inline uintptr_t& DataModelDeleterPointer = FakeDataModel::Pointer;
    inline uintptr_t& Deleter = Instance::Parent;
    inline uintptr_t& DeleterBack = Instance::ChildrenEnd;
    inline uintptr_t& OnDemandInstance = Instance::AttributeContainer;
    inline uintptr_t& RootPartR15 = Player::ModelInstance;
    inline uintptr_t& RootPartR6 = Player::ModelInstance;
    inline uintptr_t& Sandboxed = LocalScript::ByteCode;
    inline uintptr_t& RunContext = LocalScript::ByteCode;
    inline uintptr_t& ViewportSize = Camera::Position;
    inline uintptr_t& Ping = Player::Country;
    inline uintptr_t& HealthDisplayDistance = Player::MaxZoomDistance;
    inline uintptr_t& NameDisplayDistance = Player::MaxZoomDistance;
    inline uintptr_t& Tool_Grip_Position = GuiObject::LayoutOrder;
    inline uintptr_t& MeshPartColor3 = BasePart::Color3;
    inline uintptr_t& AutoJumpEnabled = Humanoid::AutoRotate;
    inline uintptr_t& BanningEnabled = DataModel::PlaceVersion;
    inline uintptr_t& ForceNewAFKDuration = DataModel::PlaceVersion;
    inline uintptr_t& InstanceCapabilities = GuiObject::BackgroundColor3;

    // Helper to trim whitespace
    inline std::string trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\r\n");
        return str.substr(first, last - first + 1);
    }

    // Helper to extract hex value from line
    inline uintptr_t extract_hex(const std::string& line) {
        size_t pos = line.find("0x");
        if (pos == std::string::npos) return 0;

        std::string hex_str;
        for (size_t i = pos; i < line.length(); i++) {
            char c = line[i];
            if (c == ';' || c == ' ' || c == '\t' || c == '\r' || c == '\n') break;
            hex_str += c;
        }

        try {
            return std::stoull(hex_str, nullptr, 16);
        }
        catch (...) {
            return 0;
        }
    }

    // Helper to extract string value from line
    inline std::string extract_string(const std::string& line) {
        size_t start = line.find('"');
        if (start == std::string::npos) return "";
        size_t end = line.find('"', start + 1);
        if (end == std::string::npos) return "";
        return line.substr(start + 1, end - start - 1);
    }

    inline bool update_offsets() {
#ifdef _WIN32
        const char* url = "https://imtheo.lol/Offsets/Offsets.hpp";
        const char* temp_file = "offsets_temp.hpp";

        HRESULT hr = URLDownloadToFileA(nullptr, url, temp_file, 0, nullptr);
        if (FAILED(hr)) return false;

        std::ifstream file(temp_file);
        if (!file.is_open()) return false;

        std::string line;
        std::string current_namespace;

        // Map of namespace::name -> pointer to variable
        std::unordered_map<std::string, uintptr_t*> offset_map = {
            // AnimationTrack
            {"AnimationTrack::Animation", &AnimationTrack::Animation},
            {"AnimationTrack::Animator", &AnimationTrack::Animator},
            {"AnimationTrack::IsPlaying", &AnimationTrack::IsPlaying},
            {"AnimationTrack::Looped", &AnimationTrack::Looped},
            {"AnimationTrack::Speed", &AnimationTrack::Speed},
            // BasePart
            {"BasePart::AssemblyAngularVelocity", &BasePart::AssemblyAngularVelocity},
            {"BasePart::AssemblyLinearVelocity", &BasePart::AssemblyLinearVelocity},
            {"BasePart::Color3", &BasePart::Color3},
            {"BasePart::Material", &BasePart::Material},
            {"BasePart::Position", &BasePart::Position},
            {"BasePart::Primitive", &BasePart::Primitive},
            {"BasePart::PrimitiveFlags", &BasePart::PrimitiveFlags},
            {"BasePart::PrimitiveOwner", &BasePart::PrimitiveOwner},
            {"BasePart::Rotation", &BasePart::Rotation},
            {"BasePart::Shape", &BasePart::Shape},
            {"BasePart::Size", &BasePart::Size},
            {"BasePart::Transparency", &BasePart::Transparency},
            {"BasePart::ValidatePrimitive", &BasePart::ValidatePrimitive},
            // ByteCode
            {"ByteCode::Pointer", &ByteCode::Pointer},
            {"ByteCode::Size", &ByteCode::Size},
            // Camera
            {"Camera::CameraSubject", &Camera::CameraSubject},
            {"Camera::CameraType", &Camera::CameraType},
            {"Camera::FieldOfView", &Camera::FieldOfView},
            {"Camera::Position", &Camera::Position},
            {"Camera::Rotation", &Camera::Rotation},
            // ClickDetector
            {"ClickDetector::MaxActivationDistance", &ClickDetector::MaxActivationDistance},
            {"ClickDetector::MouseIcon", &ClickDetector::MouseIcon},
            // DataModel
            {"DataModel::CreatorId", &DataModel::CreatorId},
            {"DataModel::GameId", &DataModel::GameId},
            {"DataModel::GameLoaded", &DataModel::GameLoaded},
            {"DataModel::JobId", &DataModel::JobId},
            {"DataModel::PlaceId", &DataModel::PlaceId},
            {"DataModel::PlaceVersion", &DataModel::PlaceVersion},
            {"DataModel::PrimitiveCount", &DataModel::PrimitiveCount},
            {"DataModel::ScriptContext", &DataModel::ScriptContext},
            {"DataModel::ServerIP", &DataModel::ServerIP},
            {"DataModel::Workspace", &DataModel::Workspace},
            // FFlags
            {"FFlags::DebugDisableTimeoutDisconnect", &FFlags::DebugDisableTimeoutDisconnect},
            {"FFlags::EnableLoadModule", &FFlags::EnableLoadModule},
            {"FFlags::PartyPlayerInactivityTimeoutInSeconds", &FFlags::PartyPlayerInactivityTimeoutInSeconds},
            {"FFlags::TaskSchedulerTargetFps", &FFlags::TaskSchedulerTargetFps},
            {"FFlags::WebSocketServiceEnableClientCreation", &FFlags::WebSocketServiceEnableClientCreation},
            // FakeDataModel
            {"FakeDataModel::Pointer", &FakeDataModel::Pointer},
            {"FakeDataModel::RealDataModel", &FakeDataModel::RealDataModel},
            // GuiObject
            {"GuiObject::BackgroundColor3", &GuiObject::BackgroundColor3},
            {"GuiObject::BorderColor3", &GuiObject::BorderColor3},
            {"GuiObject::Image", &GuiObject::Image},
            {"GuiObject::LayoutOrder", &GuiObject::LayoutOrder},
            {"GuiObject::Position", &GuiObject::Position},
            {"GuiObject::RichText", &GuiObject::RichText},
            {"GuiObject::Rotation", &GuiObject::Rotation},
            {"GuiObject::ScreenGui_Enabled", &GuiObject::ScreenGui_Enabled},
            {"GuiObject::Size", &GuiObject::Size},
            {"GuiObject::Text", &GuiObject::Text},
            {"GuiObject::TextColor3", &GuiObject::TextColor3},
            {"GuiObject::Visible", &GuiObject::Visible},
            // Humanoid
            {"Humanoid::AutoRotate", &Humanoid::AutoRotate},
            {"Humanoid::FloorMaterial", &Humanoid::FloorMaterial},
            {"Humanoid::Health", &Humanoid::Health},
            {"Humanoid::HipHeight", &Humanoid::HipHeight},
            {"Humanoid::HumanoidState", &Humanoid::HumanoidState},
            {"Humanoid::HumanoidStateID", &Humanoid::HumanoidStateID},
            {"Humanoid::Jump", &Humanoid::Jump},
            {"Humanoid::JumpHeight", &Humanoid::JumpHeight},
            {"Humanoid::JumpPower", &Humanoid::JumpPower},
            {"Humanoid::MaxHealth", &Humanoid::MaxHealth},
            {"Humanoid::MaxSlopeAngle", &Humanoid::MaxSlopeAngle},
            {"Humanoid::MoveDirection", &Humanoid::MoveDirection},
            {"Humanoid::RigType", &Humanoid::RigType},
            {"Humanoid::Walkspeed", &Humanoid::Walkspeed},
            {"Humanoid::WalkspeedCheck", &Humanoid::WalkspeedCheck},
            // Instance
            {"Instance::AttributeContainer", &Instance::AttributeContainer},
            {"Instance::AttributeList", &Instance::AttributeList},
            {"Instance::AttributeToNext", &Instance::AttributeToNext},
            {"Instance::AttributeToValue", &Instance::AttributeToValue},
            {"Instance::ChildrenEnd", &Instance::ChildrenEnd},
            {"Instance::ChildrenStart", &Instance::ChildrenStart},
            {"Instance::ClassBase", &Instance::ClassBase},
            {"Instance::ClassDescriptor", &Instance::ClassDescriptor},
            {"Instance::ClassName", &Instance::ClassName},
            {"Instance::Name", &Instance::Name},
            {"Instance::Parent", &Instance::Parent},
            // Lighting
            {"Lighting::Ambient", &Lighting::Ambient},
            {"Lighting::Brightness", &Lighting::Brightness},
            {"Lighting::ClockTime", &Lighting::ClockTime},
            {"Lighting::ColorShift_Bottom", &Lighting::ColorShift_Bottom},
            {"Lighting::ColorShift_Top", &Lighting::ColorShift_Top},
            {"Lighting::ExposureCompensation", &Lighting::ExposureCompensation},
            {"Lighting::FogColor", &Lighting::FogColor},
            {"Lighting::FogEnd", &Lighting::FogEnd},
            {"Lighting::FogStart", &Lighting::FogStart},
            {"Lighting::GeographicLatitude", &Lighting::GeographicLatitude},
            {"Lighting::OutdoorAmbient", &Lighting::OutdoorAmbient},
            // LocalScript
            {"LocalScript::ByteCode", &LocalScript::ByteCode},
            // MeshPart
            {"MeshPart::MeshId", &MeshPart::MeshId},
            {"MeshPart::Texture", &MeshPart::Texture},
            // Misc
            {"Misc::Adornee", &Misc::Adornee},
            {"Misc::AnimationId", &Misc::AnimationId},
            {"Misc::RequireLock", &Misc::RequireLock},
            {"Misc::StringLength", &Misc::StringLength},
            {"Misc::Value", &Misc::Value},
            // Model
            {"Model::PrimaryPart", &Model::PrimaryPart},
            {"Model::Scale", &Model::Scale},
            // ModuleScript
            {"ModuleScript::ByteCode", &ModuleScript::ByteCode},
            // MouseService
            {"MouseService::InputObject", &MouseService::InputObject},
            {"MouseService::MousePosition", &MouseService::MousePosition},
            {"MouseService::SensitivityPointer", &MouseService::SensitivityPointer},
            // Player
            {"Player::CameraMode", &Player::CameraMode},
            {"Player::Country", &Player::Country},
            {"Player::DisplayName", &Player::DisplayName},
            {"Player::Gender", &Player::Gender},
            {"Player::LocalPlayer", &Player::LocalPlayer},
            {"Player::MaxZoomDistance", &Player::MaxZoomDistance},
            {"Player::MinZoomDistance", &Player::MinZoomDistance},
            {"Player::ModelInstance", &Player::ModelInstance},
            {"Player::Mouse", &Player::Mouse},
            {"Player::Team", &Player::Team},
            {"Player::UserId", &Player::UserId},
            // PlayerConfigurer
            {"PlayerConfigurer::OverrideDuration", &PlayerConfigurer::OverrideDuration},
            {"PlayerConfigurer::Pointer", &PlayerConfigurer::Pointer},
            // PlayerMouse
            {"PlayerMouse::Icon", &PlayerMouse::Icon},
            {"PlayerMouse::Workspace", &PlayerMouse::Workspace},
            // PrimitiveFlags
            {"PrimitiveFlags::Anchored", &PrimitiveFlags::Anchored},
            {"PrimitiveFlags::CanCollide", &PrimitiveFlags::CanCollide},
            {"PrimitiveFlags::CanTouch", &PrimitiveFlags::CanTouch},
            // ProximityPrompt
            {"ProximityPrompt::ActionText", &ProximityPrompt::ActionText},
            {"ProximityPrompt::Enabled", &ProximityPrompt::Enabled},
            {"ProximityPrompt::GamepadKeyCode", &ProximityPrompt::GamepadKeyCode},
            {"ProximityPrompt::HoldDuration", &ProximityPrompt::HoldDuration},
            {"ProximityPrompt::KeyCode", &ProximityPrompt::KeyCode},
            {"ProximityPrompt::MaxActivationDistance", &ProximityPrompt::MaxActivationDistance},
            {"ProximityPrompt::ObjectText", &ProximityPrompt::ObjectText},
            {"ProximityPrompt::RequiresLineOfSight", &ProximityPrompt::RequiresLineOfSight},
            // RenderView
            {"RenderView::DeviceD3D11", &RenderView::DeviceD3D11},
            {"RenderView::VisualEngine", &RenderView::VisualEngine},
            // RunService
            {"RunService::HeartbeatFPS", &RunService::HeartbeatFPS},
            {"RunService::HeartbeatTask", &RunService::HeartbeatTask},
            // Sky
            {"Sky::MoonAngularSize", &Sky::MoonAngularSize},
            {"Sky::MoonTextureId", &Sky::MoonTextureId},
            {"Sky::SkyboxBk", &Sky::SkyboxBk},
            {"Sky::SkyboxDn", &Sky::SkyboxDn},
            {"Sky::SkyboxFt", &Sky::SkyboxFt},
            {"Sky::SkyboxLf", &Sky::SkyboxLf},
            {"Sky::SkyboxOrientation", &Sky::SkyboxOrientation},
            {"Sky::SkyboxRt", &Sky::SkyboxRt},
            {"Sky::SkyboxUp", &Sky::SkyboxUp},
            {"Sky::StarCount", &Sky::StarCount},
            {"Sky::SunAngularSize", &Sky::SunAngularSize},
            {"Sky::SunTextureId", &Sky::SunTextureId},
            // SpecialMesh
            {"SpecialMesh::MeshId", &SpecialMesh::MeshId},
            {"SpecialMesh::Scale", &SpecialMesh::Scale},
            // StatsItem
            {"StatsItem::Value", &StatsItem::Value},
            // TaskScheduler
            {"TaskScheduler::FakeDataModelToDataModel", &TaskScheduler::FakeDataModelToDataModel},
            {"TaskScheduler::JobEnd", &TaskScheduler::JobEnd},
            {"TaskScheduler::JobName", &TaskScheduler::JobName},
            {"TaskScheduler::JobStart", &TaskScheduler::JobStart},
            {"TaskScheduler::MaxFPS", &TaskScheduler::MaxFPS},
            {"TaskScheduler::Pointer", &TaskScheduler::Pointer},
            {"TaskScheduler::RenderJobToFakeDataModel", &TaskScheduler::RenderJobToFakeDataModel},
            {"TaskScheduler::RenderJobToRenderView", &TaskScheduler::RenderJobToRenderView},
            // Team
            {"Team::BrickColor", &Team::BrickColor},
            // Textures
            {"Textures::Decal_Texture", &Textures::Decal_Texture},
            {"Textures::Texture_Texture", &Textures::Texture_Texture},
            // VisualEngine
            {"VisualEngine::Dimensions", &VisualEngine::Dimensions},
            {"VisualEngine::Pointer", &VisualEngine::Pointer},
            {"VisualEngine::ToDataModel1", &VisualEngine::ToDataModel1},
            {"VisualEngine::ToDataModel2", &VisualEngine::ToDataModel2},
            {"VisualEngine::ViewMatrix", &VisualEngine::ViewMatrix},
            // Workspace
            {"Workspace::CurrentCamera", &Workspace::CurrentCamera},
            {"Workspace::DistributedGameTime", &Workspace::DistributedGameTime},
            {"Workspace::Gravity", &Workspace::Gravity},
            {"Workspace::GravityContainer", &Workspace::GravityContainer},
            {"Workspace::PrimitivesPointer1", &Workspace::PrimitivesPointer1},
            {"Workspace::PrimitivesPointer2", &Workspace::PrimitivesPointer2},
            {"Workspace::ReadOnlyGravity", &Workspace::ReadOnlyGravity},
        };

        while (std::getline(file, line)) {
            line = trim(line);

            // Skip empty lines and comments
            if (line.empty() || line[0] == '/' || line[0] == '*') continue;

            // Check for ClientVersion
            if (line.find("ClientVersion") != std::string::npos && line.find("=") != std::string::npos) {
                std::string version = extract_string(line);
                if (!version.empty()) {
                    ClientVersion = version;
                }
                continue;
            }

            // Check for namespace start
            if (line.find("namespace ") != std::string::npos && line.find("{") != std::string::npos) {
                size_t ns_start = line.find("namespace ") + 10;
                size_t ns_end = line.find_first_of(" {", ns_start);
                if (ns_end != std::string::npos) {
                    current_namespace = line.substr(ns_start, ns_end - ns_start);
                }
                continue;
            }

            // Check for namespace end
            if (line == "}") {
                current_namespace.clear();
                continue;
            }

            // Parse offset lines: "inline constexpr uintptr_t OffsetName = 0xXXX;"
            if (!current_namespace.empty() && line.find("inline") != std::string::npos &&
                line.find("uintptr_t") != std::string::npos && line.find("=") != std::string::npos) {

                // Extract offset name
                size_t name_start = line.find("uintptr_t") + 9;
                while (name_start < line.length() && (line[name_start] == ' ' || line[name_start] == '\t')) {
                    name_start++;
                }

                size_t name_end = line.find_first_of(" =", name_start);
                if (name_end != std::string::npos) {
                    std::string offset_name = line.substr(name_start, name_end - name_start);
                    std::string full_name = current_namespace + "::" + offset_name;

                    uintptr_t value = extract_hex(line);

                    auto it = offset_map.find(full_name);
                    if (it != offset_map.end()) {
                        *(it->second) = value;
                    }
                }
            }
        }

        file.close();
        std::remove(temp_file);
        return true;
#else
        return false;
#endif
    }
}