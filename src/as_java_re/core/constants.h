/** @file constants.h
 * @brief Collect every named constant: canvas, timing, original IDs, gameplay rules, layout, colors and settings.
 *
 * Asset data (sprites, compositions, text, animations) lives in assets/. This file keeps the values that the
 * original code fixed in its rules, plus the IDs those rules read or write. Change values here only when the
 * corresponding original behavior is meant to change; regression traces compare against the original game.
 */
#pragma once
#include <array>

namespace alien {
// ---------------------------------------------------------------------------------------------
// Canvas and timing
// ---------------------------------------------------------------------------------------------
inline constexpr int ScreenWidth = 240;
inline constexpr int ScreenHeight = 320;
inline constexpr int TileSize = 16;
inline constexpr int TickMilliseconds = 80;
inline constexpr float TickSeconds = 0.08f;
// Longest desktop frame consumed at once; longer stalls do not fast-forward the world.
inline constexpr float MaxFrameSeconds = 0.25f;
inline constexpr float CameraTransitStepSeconds = 0.05f;
inline constexpr float DeathParticleStepSeconds = 0.08f;
inline constexpr float DeathWipeStepSeconds = 0.1f;
// Render-only smoothing between ticks; per-tick moves longer than this are teleports and snap.
inline constexpr int InterpolationMaxStep = 64;
// Initial and test random seed of the original Java LCG.
inline constexpr unsigned long long DefaultRandomSeed = 12345;
// java.util.Random linear congruential generator.
namespace java_random {
inline constexpr unsigned long long Multiplier = 25214903917ULL;
inline constexpr unsigned long long Increment = 11;
inline constexpr unsigned long long Mask = 281474976710655ULL;
inline constexpr int StateBits = 48;
}

// ---------------------------------------------------------------------------------------------
// Original object types (level entity records and runtime objects)
// ---------------------------------------------------------------------------------------------
namespace object_type {
// Records below this value spawn enemies of that type.
inline constexpr int FirstObjectType = 10;
inline constexpr int WeaponPickup = 10;        // Records 10-13: machine gun, flamethrower, rocket launcher, blaster.
inline constexpr int WeaponPickupLast = 13;
inline constexpr int AmmoPickup = 14;          // Records 14-17 refill the matching weapon.
inline constexpr int AmmoPickupLast = 17;
inline constexpr int ScorePickup = 18;
inline constexpr int ExplosivePickup = 19;
inline constexpr int Medkit = 20;
inline constexpr int ArmorPickup = 21;         // Records 21-22: light armor, armor with helmet.
inline constexpr int ArmorPickupLast = 22;
inline constexpr int Generator = 25;           // Draws arcs once the power switch is used.
inline constexpr int GasCylinder = 26;         // Breakables 26-32 block shots and crumble.
inline constexpr int Barrel = 27;
inline constexpr int CrushedBarrel = 28;
inline constexpr int Drum = 29;                // Two cells wide.
inline constexpr int ToxicTank = 30;           // Tanks 30-31 leave a floor flame.
inline constexpr int ToxicTankLow = 31;
inline constexpr int Crate = 32;               // Stackable.
inline constexpr int BreakableFirst = 26;
inline constexpr int BreakableLast = 32;
inline constexpr int DestroyedHive = 33;
inline constexpr int VerticalPlatform = 34;
inline constexpr int HorizontalPlatform = 35;
inline constexpr int TeleporterSource = 36;    // Enemies emerge in place before acting.
inline constexpr int HiveSource = 37;          // Becomes DestroyedHive after the explosion.
inline constexpr int PowerSwitch = 38;
inline constexpr int UsedSwitch = 39;
inline constexpr int FloorFlame = 40;          // Burning puddle; hurts on contact and burns out.
inline constexpr int Terminal = 41;            // Records 41-43: terminal variants.
inline constexpr int TerminalLast = 43;
inline constexpr int PoweredTerminalRecord = 42;
inline constexpr int DamagedTerminalRecord = 43;
inline constexpr int CeilingLight = 100;       // Light markers are not drawn and have no parameters.
inline constexpr int WallLight = 101;
}

// Object states shared by sources and lights.
namespace source_state {
inline constexpr int Idle = 0;
inline constexpr int Armed = 1;
inline constexpr int Destroyed = 2;
}
// Terminal variants after loading: 0 unpowered, 1 powered, 2 exit without a door.
namespace terminal_variant {
inline constexpr int Unpowered = 0;
inline constexpr int Powered = 1;
inline constexpr int Doorless = 2;
}

// ---------------------------------------------------------------------------------------------
// Original world tile ids read or written by the rules (identical in all three themes)
// ---------------------------------------------------------------------------------------------
namespace tile_id {
inline constexpr int Empty = 0;
inline constexpr int BloodSplash = 52;
inline constexpr int BloodSmear = 76;
inline constexpr int BloodPool = 77;
inline constexpr int DebrisA = 78;             // 78-80 reuse sprite parts as debris.
inline constexpr int DebrisB = 79;
inline constexpr int DebrisC = 80;
inline constexpr int Wall = 105;
inline constexpr int UnlitLamp = 107;
inline constexpr int WallHoleTop = 112;
inline constexpr int WallHole = 113;
inline constexpr int WallHoleBottom = 114;
inline constexpr int WallHoleRight = 115;
inline constexpr int WallHoleLeft = 116;
inline constexpr int CeilingLampLit = 117;
inline constexpr int WallLampLit = 118;
inline constexpr int WallLampGlow = 119;
inline constexpr int LitWallAboveFloor = 120;
inline constexpr int PoweredTerminalWall = 121;
inline constexpr int WallVariantLast = 124;    // 120-124 wall variants that holes may replace.
inline constexpr int MarkedWallA = 122;
inline constexpr int MarkedWallB = 126;
inline constexpr int BrokenWallBase = 133;
inline constexpr int UsedSocket = 137;
inline constexpr int Floor = 145;
inline constexpr int LitFloorLeft = 146;
inline constexpr int LitFloorCenter = 147;
inline constexpr int LitFloorRight = 148;
inline constexpr int FloorHoleLeft = 149;      // 149-151 damaged floor.
inline constexpr int FloorHoleCenter = 150;
inline constexpr int FloorHoleRight = 151;
inline constexpr int PillarTop = 152;
inline constexpr int BrokenPillarTop = 153;
inline constexpr int Pillar = 158;
inline constexpr int FloorLast = 169;          // Composite tiles on 145-169 count as floor.
inline constexpr int CompositeFirst = 170;     // 170-219 composite slots: floors grow up, walls grow down.
inline constexpr int CompositeLast = 219;
inline constexpr int IdLimit = 220;
// Lit floor tiles map to player light parts: tile - offset, mirrored as sum - part.
inline constexpr int LightPartOffset = 134;
inline constexpr int MirroredLightPartSum = 26;
}

// ---------------------------------------------------------------------------------------------
// Sound ids (index into audio/sounds.json), named by the event that plays them
// ---------------------------------------------------------------------------------------------
namespace sound {
inline constexpr int PowerOn = 0;
inline constexpr int Pickup = 1;
inline constexpr int Explosion = 2;
inline constexpr int Hurt = 6;                 // Player damage; also the death cry of enemy type 1.
inline constexpr int TankBurst = 7;
inline constexpr int BulletImpact = 8;
inline constexpr int PillarHit = 9;
inline constexpr int LevelComplete = 10;
inline constexpr int LevelMusic = 11;          // Level start and pause menu.
inline constexpr int EnemyDeathTypes2And4 = 12;
inline constexpr int EnemyDeathTypes3And5 = 13;
inline constexpr int TerminalTravel = 14;
inline constexpr int SourceExplosion = 15;
inline constexpr int MainMenuMusic = 16;
// Fire sound per weapon: pistol, machine gun, flamethrower, rocket launcher, blaster.
inline constexpr std::array<int, 5> WeaponFire{3, 4, 8, 5, 5};
}

// Message keys of localization/<language>/interface.json.
namespace message_key {
inline constexpr char FindDynamitePlace[] = "find_dynamite_place";
inline constexpr char TerminalTravel[] = "terminal_travel";
inline constexpr char SwitchWeapon[] = "switch_weapon";
inline constexpr char AmmoPickedUp[] = "ammo_picked_up";
inline constexpr char PlantDynamite[] = "plant_dynamite";
inline constexpr char LightArmor[] = "light_armor";
inline constexpr char HeavyArmor[] = "heavy_armor";
inline constexpr char AreaCleared[] = "area_cleared";
inline constexpr char LevelFailed[] = "level_failed";
inline constexpr char LevelComplete[] = "level_complete";
inline constexpr char ExitUnavailable[] = "exit_unavailable";
inline constexpr char GameComplete[] = "game_complete";
}

// Tutorial bits stored in saves; bit numbers come from the original message rows.
namespace tutorial {
inline constexpr int Terminal = 1;
inline constexpr int Explosive = 4;
inline constexpr int Weapon = 6;
inline constexpr int Ammo = 7;
inline constexpr int Source = 8;
inline constexpr int ArmorBase = 8;            // Armor variant 1-2 uses bits 9-10.
}

// ---------------------------------------------------------------------------------------------
// Gameplay rules
// ---------------------------------------------------------------------------------------------
namespace rules {
inline constexpr int PlayerType = 0;           // Actor type 0 is the player; enemies use 1-5.
inline constexpr int WeaponCount = 5;
inline constexpr int Pistol = 0;
inline constexpr int Flamethrower = 2;
inline constexpr int PistolAmmo = 9999;
// Weapons 0-2 have their own body pose; heavier weapons share pose 2.
inline constexpr int PoseCount = 3;
inline constexpr int DifficultyCount = 3;
inline constexpr int DefaultDifficulty = 1;
inline constexpr int DifficultyHealthPercent = 20;
inline constexpr int MaxHealth = 100;

// Player states and air states of the original actor record.
inline constexpr int StateIdle = 0;
inline constexpr int StateWalk = 1;
inline constexpr int StateAttack = 2;
inline constexpr int StateAir = 3;
inline constexpr int AirGrounded = 0;
inline constexpr int AirJumping = 1;
inline constexpr int AirLanding = 2;           // Not set by the current rules; jump and crouch still accept it.
inline constexpr int AirFalling = 3;

// Damage.
inline constexpr int LightArmor = 1;
inline constexpr int HeavyArmor = 2;
inline constexpr int LightArmorNumerator = 5;  // Light armor: ceil(amount * 5 / 16).
inline constexpr int LightArmorDenominator = 16;
inline constexpr int FatalFallSpeed = -24;
inline constexpr int DamagingFallSpeed = -20;
inline constexpr int FallDamageBase = 5;
inline constexpr int FallDamagePerSpeed = 10;
inline constexpr int FatalDamage = 100;

// Player movement and firing.
inline constexpr int StepUpTolerance = 2;      // Highest ledge an actor walks onto.
inline constexpr int MuzzleOffsetX = 12;
inline constexpr int MuzzleHeight = 24;
inline constexpr int PistolMuzzleHeight = 35;
inline constexpr int FlamethrowerMuzzleHeight = 30;
inline constexpr int PistolAim = -1;           // Vertical projectile direction.
inline constexpr int FlamethrowerAim = -2;
inline constexpr int RecoilBobFrames = 2;      // Muzzle bob cycles 2, 1, 0.
inline constexpr int PlayerHalfWidth = 11;
inline constexpr int PlatformFootReach = 2;
inline constexpr int PlatformFootHeight = 4;
inline constexpr int PlatformRiderOffset = 10;
inline constexpr int PlatformBoardVelocity = 1;
inline constexpr int PlatformProbeBelow = 1;   // Rows probed ahead of moving platforms.
inline constexpr int PlatformProbeAbove = 4;
inline constexpr int PlatformProbeAhead = 1;

// Enemies.
inline constexpr int MaxEnemies = 29;
inline constexpr int MaxEnemyType = 5;
inline constexpr int EmptySpawnType = 15;
inline constexpr int RangedEnemyType = 4;      // Types 4-5 shoot projectiles.
inline constexpr int HeavyRangedEnemyType = 5;
inline constexpr int ChargingEnemyType = 3;    // Lunges two pixels while attacking.
inline constexpr int ChargeOffset = 2;
inline constexpr int SpawnShotTypeOffset = 3;  // Records 6+ spawn type 5 with projectile record - 3.
inline constexpr int EnemyWakeRangeX = 480;
inline constexpr int EnemyWakeRangeY = 640;
inline constexpr int ThreatRange = 360;
inline constexpr int RangedAttackReach = 210;
inline constexpr int MeleeCooldownMs = 1000;
inline constexpr int EmergeDurationMs = 1000;
inline constexpr int EmergeBehavior = 100;
inline constexpr int RetreatMinDistance = 120;
inline constexpr int RetreatChancePercent = 20;
inline constexpr int RetreatResumeMs = 500;
inline constexpr int TurnChancePercent = 1;
inline constexpr int BloodSpawnHeight = 24;
inline constexpr int RangedMuzzleHeight = 20;
inline constexpr int HeavyRangedMuzzleHeight = 40;
inline constexpr int OffscreenSpawnScatter = 60;
inline constexpr int VisibilityMargin = 16;    // Off-screen distance still treated as visible.
inline constexpr int SpawnVisibleMarginRight = 64;
inline constexpr int SpawnVisibleMarginY = 32;
inline constexpr int EnemySpawnLift = 8;

// Score.
inline constexpr int KillScorePerType = 5;
inline constexpr int PickupScore = 10;
inline constexpr int ScoreFlashTicks = 7;

// Level loading and decoration.
inline constexpr int ObjectSpawnLift = 15;     // Objects sit 15px below the cell top.
inline constexpr int CorpsesPerLevel = 5;      // Times (level - 1).
inline constexpr int FlamesPerLevel = 3;       // Times (level - 1).
inline constexpr int DecorationMargin = 2;
inline constexpr int CrateJitterX = 3;
inline constexpr int CrateJitterY = 4;
inline constexpr int CrateStackGap = 12;
inline constexpr int CrateFloorSupport = 8;
inline constexpr int TallBreakableCapHeight = 4;   // Collision of the cell above tall breakables.
inline constexpr int LowBreakableHeight = 12;      // Collision of drums and low tanks.
inline constexpr int TerminalOffsetX = 6;
inline constexpr int PickupBobFrames = 16;     // Floating pickups step two frames at a time.
inline constexpr std::array<int, 8> PickupBob{0, 1, 0, -1, 0, -1, 0, 1};

// Mechanisms.
inline constexpr int TerminalDoorStep = 6;
inline constexpr int TerminalDoorOpen = -48;
inline constexpr int TerminalArrivalOffsetX = 16;
inline constexpr int TerminalArrowFrames = 4;
inline constexpr int ExitTerminalGroup = 0;     // Terminal group 0 is the level exit.
inline constexpr int GeneratorSpawnIntervalMs = 5000;
inline constexpr int GeneratorSpawnPattern = 4;
inline constexpr int DynamiteCountdownMs = 5800;
inline constexpr int DynamiteDigitShift = 10;  // Countdown digit = remaining ms >> 10.
inline constexpr int DynamiteFirstDigit = 9;
inline constexpr int SourceBlastBursts = 15;
inline constexpr int SourceBlastDamage = 1000;
inline constexpr int SourceBlastRadius = 72;
inline constexpr int SourceCenterOffset = 8;   // Spawn and blast center relative to the source origin.
inline constexpr int SwitchArrowLastFrame = 3;
inline constexpr int LightFlickerChance = 12;  // Out of 256 per tick.
inline constexpr int LightFlickerKeep = 10;    // Out of 16 per light.
inline constexpr int CeilingLightReach = 3;

// Projectiles (record ids of projectile_parameters.json).
inline constexpr int FlameProjectile = 2;
inline constexpr int RocketProjectile = 3;
inline constexpr int ExplosionProjectile = 5;
inline constexpr int ImpactProjectile = 6;
inline constexpr int BloodProjectile = 7;
inline constexpr int DebrisProjectile = 8;
inline constexpr int SmokeProjectile = 9;
inline constexpr int FirstHeavyProjectile = 2; // Types below 2 are bullets that stop on the first hit.
inline constexpr int FirstPillarProofProjectile = 5; // Types from 5 do not damage pillars.
inline constexpr int FirstEffectProjectile = 6;      // Types from 6 are effects that never hit actors.
inline constexpr int DefaultProjectileLife = 3;
inline constexpr int RocketLife = 1;
inline constexpr int SmokeLife = 4;
inline constexpr int ParticleLastFrame = 2;    // Blood and debris cycle frames 2, 1, 0.
inline constexpr int ProjectileGravity = 2;
inline constexpr int ImpactInset = 5;
inline constexpr int WallImpactInset = 4;
inline constexpr int ExplosionRadius = 16;
inline constexpr int GasCylinderBlastRadius = 32;
inline constexpr int PillarBreakDamage = 100;
inline constexpr int PillarRubbleHeight = 10;
inline constexpr int PillarDamageMask = 255;  // Pillar damage accumulates in its collision byte.
inline constexpr int PillarDebrisFastSpeed = 5;
inline constexpr int PillarDebrisSlowSpeed = 4;
inline constexpr int PillarDebrisSpreadY = 10;
inline constexpr int BloodHorizontalBits = 3;
inline constexpr int BloodHorizontalCenter = 4;
inline constexpr int CrateDebrisPieces = 10;
inline constexpr int CrateDebrisSpreadX = 5;
inline constexpr int CrateDebrisSpreadY = 9;
// Effects spawned when a breakable object is destroyed, relative to its origin.
inline constexpr int BreakFlameOffsetX = 7;
inline constexpr int BreakEffectOffsetX = 8;
inline constexpr int BreakSmokeOffsetY = 36;
inline constexpr int BreakBlastOffsetY = 24;
inline constexpr int BreakBlastSpreadY = 16;

// Screen shake amplitudes.
inline constexpr int SourceBlastShake = 8;
inline constexpr int ObjectBreakShake = 3;
inline constexpr int PillarBreakShake = 5;

// Camera, in original bottom-left coordinates.
inline constexpr int CameraLead = 32;
inline constexpr int CameraLookDownMargin = 48;
inline constexpr int CameraFollowSpeed = 20;
inline constexpr int CameraTransitSpeed = 40;

// HUD animation.
inline constexpr int HealthFlashTicks = 3;
inline constexpr int DamageFlashColor = 0xff0000;
inline constexpr int DamageFlashStep = 0x400000;
inline constexpr int HealFlashColor = 0x00ff00;
inline constexpr int HealFlashStep = 0x002000;
inline constexpr int WeaponPanelSpeed = 5;
inline constexpr int WeaponPanelSlide = 20;      // Pixels the weapon panel slides down.
inline constexpr int WeaponPanelHoldMs = 3000;
inline constexpr int MedkitFrames = 5;
inline constexpr int FlameFrameHold = 1;        // Flames toggle once the tick counter exceeds this.

// Death sequence.
inline constexpr int DeathParticles = 5;
inline constexpr int DeathParticleSteps = 20;
inline constexpr int DeathWipeColumnWidth = 3;
inline constexpr int DeathWipeMaxStep = 40;
}

// Desktop cheat typed during play; toggles invulnerability and all weapons.
inline constexpr char CheatCode[] = "chgod";

// ---------------------------------------------------------------------------------------------
// Desktop flow
// ---------------------------------------------------------------------------------------------
namespace flow {
inline constexpr float PublisherLogoSeconds = 3.0f;
inline constexpr float IntroSeconds = 5.0f;
// Sound prompt blinks: 200ms blank, then 1000ms visible.
inline constexpr float SoundPromptCycleSeconds = 1.2f;
inline constexpr float SoundPromptBlankSeconds = 0.2f;
inline constexpr float MenuAnimationStepSeconds = 0.03f;
inline constexpr int MenuPhasePerStep = 4;
inline constexpr float SelectionSpreadSeconds = 0.3f;
inline constexpr int SelectionSpreadSteps = 10;
inline constexpr float MessageOpenSeconds = 0.15f;
inline constexpr float MessageCharacterSeconds = 0.05f;
inline constexpr int PageWheelStep = 28;
inline constexpr float PageScrollPerSecond = 100.0f;
inline constexpr int PageJump = 240;
}

// ---------------------------------------------------------------------------------------------
// Drawing layout on the 240x320 canvas
// ---------------------------------------------------------------------------------------------
namespace layout {
// Soft-key bar at the bottom of menus and prompts.
inline constexpr int SoftKeyBarY = 310;
inline constexpr int SoftKeyTextY = 312;
inline constexpr int SoftKeyBarHeight = 10;
inline constexpr int SoftKeyClickY = 309;
inline constexpr int SoftKeyLeftClickWidth = 60;
inline constexpr int SoftKeyRightClickX = 185;
inline constexpr int PromptClickY = 302;
inline constexpr int PromptRightClickX = 180;
// Continue arrow at the bottom right of overlays: a short stack of lines above a triangle.
inline constexpr int ContinueArrowX = 226;
inline constexpr int ContinueArrowY = 311;
inline constexpr int ContinueArrowWidth = 11;
inline constexpr int ContinueStemLeft = 228;
inline constexpr int ContinueStemRight = 234;
inline constexpr int ContinueStemBottom = 309;
inline constexpr int ContinueStemRows = 3;

// Menus.
inline constexpr int MenuLeft = 4;
inline constexpr int MenuTop = 82;
inline constexpr int MenuWidth = 162;
inline constexpr int MenuHeight = 224;
inline constexpr int PauseMenuLeft = 75;
inline constexpr int PauseMenuTop = 118;
inline constexpr int PauseMenuWidth = 90;
inline constexpr int PauseMenuHeight = 84;
inline constexpr int MenuTitleHeight = 12;
inline constexpr int MenuItemHeight = 12;
inline constexpr int MenuOverlayX = 4;
inline constexpr int MenuOverlayY = 4;
inline constexpr int MenuTextHeight = 10;
inline constexpr int MenuMarkerWidth = 5;     // Triangles at the top and bottom center of a menu.
inline constexpr int MenuSpreadClipHeight = 6;
inline constexpr int MenuSpreadHalfWidth = 81;
inline constexpr int PauseSpreadHalfWidth = 45;

// Intro and prompt.
inline constexpr int IntroLogoX = 67;
inline constexpr int IntroLogoY = 131;
inline constexpr int SoundPromptCenterX = 120;
inline constexpr int SoundPromptFirstY = 152;
inline constexpr int SoundPromptSecondY = 161;

// Message box, centered on the canvas; it grows in three steps before typing its text.
inline constexpr int MessageCenterX = 120;
inline constexpr int MessageCenterY = 160;
inline constexpr int MessageMinWidth = 40;
inline constexpr int MessageLineHeight = 9;
inline constexpr int MessagePadding = 7;
inline constexpr int MessageTextTop = 5;
inline constexpr int MessageGrowSteps = 2;
inline constexpr int MessageGrowWidth = 10;
inline constexpr int MessageGrowHeight = 5;
inline constexpr int MessageCursorWidth = 5;
inline constexpr int MessageCursorHeight = 8;
inline constexpr int MessageArrowY = 305;

// Rich text pages: title bar, scrolling body, bottom bar, and a scroll track on the right.
inline constexpr int PageTop = 19;
inline constexpr int PageHeight = 286;
inline constexpr int PageBottomBarY = 305;
inline constexpr int PageBottomBarHeight = 15;
inline constexpr int PageTitleY = 8;
inline constexpr int PageTextLeft = 10;
inline constexpr int PageTextRightInset = 11;
inline constexpr int PageTextWidth = 220;
inline constexpr int PageTextBottom = 302;
inline constexpr int PageFirstLine = 22;
inline constexpr int PageLineGap = 7;
inline constexpr int PageBackgroundX = 96;
inline constexpr int PageBackgroundY = 127;
inline constexpr int PageGradientWidth = 30;
inline constexpr int PageScrollX = 235;
inline constexpr int PageScrollTrackTop = 32;
inline constexpr int PageScrollTrackHeight = 260;
inline constexpr int PageScrollMinThumb = 2;
// Original scroll metrics: content height excludes 35px of margins; the view shows 274-280px of it.
inline constexpr int PageContentMargin = 35;
inline constexpr int PageLastRowReach = 274;
inline constexpr int PageScrollableHeight = 280;

// HUD: ammo panel (top left), weapon panel sliding below it, score panel (top right), health (bottom left).
inline constexpr int AmmoPanelWidth = 51;
inline constexpr int PanelHeight = 14;
inline constexpr int WeaponPanelHeight = 21;
inline constexpr int WeaponPanelTop = -20;
inline constexpr int WeaponIconCenterX = 25;
inline constexpr int WeaponIconTop = -19;
inline constexpr int AmmoBarX = 3;
inline constexpr int AmmoBarY = 4;
inline constexpr int AmmoBarWidth = 16;
inline constexpr int AmmoBarHeight = 6;
inline constexpr int AmmoBarLow = 5;
inline constexpr int AmmoTextX = 21;
inline constexpr int ScorePanelWidth = 46;
inline constexpr int ScoreIconInsetX = 43;
inline constexpr int ScoreIconY = 2;
inline constexpr int ScoreTextInsetX = 3;
inline constexpr int PanelTextY = 4;
inline constexpr int HealthTextX = 16;
inline constexpr int HealthTextBottom = 17;
inline constexpr int AmmoIconCenterX = 40;
inline constexpr int AmmoIconBottom = 5;
inline constexpr int ExplosiveRowY = 15;
inline constexpr int ExplosiveRowHeight = 12;
inline constexpr int ExplosiveIconInsetX = 14;
inline constexpr int ExplosiveIconSpacing = 14;
inline constexpr int DigitCount = 4;            // Ammo and score are zero padded to four digits.

// Objects.
inline constexpr int CountdownTextX = 4;
inline constexpr int CountdownTextY = -38;
inline constexpr int ExitLabelX = 10;
inline constexpr int ExitLabelY = -62;
inline constexpr int TerminalArrowX = 30;
inline constexpr int TerminalArrowY = -44;
inline constexpr int TerminalArrowWidth = 7;
inline constexpr int GlyphHeight = 6;
inline constexpr int FontFirstCharacter = 32;  // Glyph 0 is the space; ids follow the byte value.
inline constexpr int ProjectileCullMargin = 20;
}

// Procedural object effects, relative to the object's top-left drawing point.
namespace effect_shape {
// Generator status lamps: three short horizontal lines.
inline constexpr int LampLeft = 12;
inline constexpr int LampRight = 17;
inline constexpr int LampTop = -19;
inline constexpr int LampSpacing = 2;
inline constexpr int LampCount = 3;
// Generator arcs once powered; each end jitters by the random arc values.
inline constexpr int InnerArcLeft = 11;
inline constexpr int InnerArcRight = 18;
inline constexpr int InnerArcY = -50;
inline constexpr int OuterArcLeft = 4;
inline constexpr int OuterArcRight = 25;
inline constexpr int OuterArcY = -57;
// Power switch: two converging red laser fans.
inline constexpr int LaserRows = 3;
inline constexpr int LaserLeftX = -2;
inline constexpr int LaserRightX = 33;
inline constexpr int LaserTop = -11;
inline constexpr int LaserBottom = -6;
}

// 0xRRGGBB colors used by procedural drawing.
namespace color {
inline constexpr unsigned int PixelGreen = 0x00ff00;
inline constexpr unsigned int HudPanel = 0x282830;
inline constexpr unsigned int HudBorder = 0x999d93;
inline constexpr unsigned int AmmoFull = 0x00ff00;
inline constexpr unsigned int AmmoLow = 0xff7700;
inline constexpr unsigned int AmmoStripeMask = 0x8f8f8f;
inline constexpr unsigned int MenuScanline = 0x005826;
inline constexpr unsigned int PageBackground = 0x002c13;
inline constexpr unsigned int DeathWipe = 0x800000;
inline constexpr unsigned int Failure = 0xff0000;
inline constexpr unsigned int SwitchLaser = 0xff0000;
}

// ---------------------------------------------------------------------------------------------
// Audio
// ---------------------------------------------------------------------------------------------
inline constexpr int AudioSampleRate = 44100;
namespace audio_settings {
inline constexpr int ChannelCount = 2;
inline constexpr int SampleBits = 32;
inline constexpr double Gain = 0.2;
inline constexpr int Polyphony = 256;
inline constexpr int StreamBufferFrames = 512;
}

// ---------------------------------------------------------------------------------------------
// Desktop configuration file (config.cfg beside the executable)
// ---------------------------------------------------------------------------------------------
namespace config {
// Keep file names, CFG keys, and defaults together for future changes.
inline constexpr char FileName[] = "config.cfg";
inline constexpr char TitleKey[] = "Title";
inline constexpr char ScreenXKey[] = "DefaultScreenX";
inline constexpr char ScreenYKey[] = "DefaultScreenY";
inline constexpr char ScalarKey[] = "Scalar";
inline constexpr char MaxFpsKey[] = "MaxFps";

inline constexpr char DefaultTitle[] = "Alien Shooter 2D";
inline constexpr int DefaultScreenX = 240;
inline constexpr int DefaultScreenY = 320;
inline constexpr int DefaultScalar = 3;
// Keep the initial window this far inside the monitor.
inline constexpr int WindowMargin = 80;
// Desktop frame rate limit; world logic still advances every TickSeconds.
inline constexpr int DefaultMaxFps = 120;
}

// Save files beside the executable.
namespace save_file {
inline constexpr int Version = 1;
inline constexpr char Primary[] = "progress.json";
inline constexpr char Backup[] = "progress.bak";
inline constexpr char Temporary[] = "progress.tmp";
inline constexpr char Corrupt[] = "progress.corrupt.json";
}
}
