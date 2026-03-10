#pragma once

#include "API/ARK/Ark.h"
#include <chrono>
#include <map>
#include <vector>
#include <string>

// Helper structs for accessing game types
struct FAnimNotifyEvent
{
    FName NotifyNameField() { return *GetNativePointerField<FName*>(this, "FAnimNotifyEvent.NotifyName"); }
};

struct UAnimInstance : UObject
{
    TArray<FAnimNotifyEvent*> AnimNotifiesField() { return *GetNativePointerField<TArray<FAnimNotifyEvent*>*>(this, "UAnimInstance.AnimNotifies"); }
};

namespace KillTracker
{
    // Shared data structures
    struct ShotStats {
        int totalShots = 0;
        int playerHits = 0;
        int dinoHits = 0;
        float totalDamageToPlayers = 0.0f;
        std::string lastWeaponUsed = "";
        std::string playerName = "";
    };
    extern std::map<uint64, ShotStats> PlayerShotStats;

    struct BolaStats {
        int totalChucks = 0;
        int playerHits = 0;
        int dinoHits = 0;
        int misses = 0;
        std::string playerName = "";
    };
    extern std::map<uint64, BolaStats> PlayerBolaStats;

    struct ResourceStats {
        std::string playerName;
        int metal = 0;
        int flint = 0;
        int stone = 0;
        int pearl = 0;
        int blackPearl = 0;
        int oil = 0;
        int sulfur = 0;
        int fiber = 0;
        int crystal = 0;
        int wood = 0;
        int thatch = 0;
        int element = 0;
        int elementShards = 0;
        int charcoal = 0;
        int organicPoly = 0;
        int hide = 0;
        int keratin = 0;
        int chitin = 0;
        int megachelonShells = 0;
        int obsidian = 0;
    };
    extern std::map<uint64, ResourceStats> PlayerResourceStats;

    struct BodyPartHitStats {
        std::string playerName;
        int headHits = 0;
        int neckHits = 0;
        int chestHits = 0;
        int stomachHits = 0;
        int armHits = 0;
        int handHits = 0;
        int legHits = 0;
        int footHits = 0;
        int otherHits = 0;
    };
    extern std::map<uint64, BodyPartHitStats> PlayerBodyPartStats;

    struct RecentShot {
        uint64 steamId;
        std::string weaponName;
        std::string playerName;
        FVector location;
        std::chrono::system_clock::time_point shotTime;
        bool hasHit = false;
        bool actuallyFired = false;
        std::string hitTarget = "";
        std::string hitTargetName = "";
        std::string boneName = "";
        std::chrono::system_clock::time_point missCheckTime;
    };
    extern std::vector<RecentShot> RecentShots;

    struct HarvestInfo {
        std::chrono::system_clock::time_point harvestTime;
        std::string playerName;
        uint64 steamId;
        std::string harvestMethod;
    };
    extern std::map<uint64, HarvestInfo> RecentHarvesters;

    struct BossFightInfo {
        std::chrono::system_clock::time_point startTime;
        std::string bossName;
        std::string difficulty;
        bool completed = false;
    };
    extern std::map<uint64, BossFightInfo> PlayersInBossFight;

    struct KillEvent {
        std::string killerName;
        uint64 killerSteamId;
        std::string victimName;
        uint64 victimSteamId;
        std::string weaponName;
        std::string timestamp;
        bool onDino;
    };
    extern std::vector<KillEvent> KillEvents;

    struct DeathEvent {
        std::string victimName;
        uint64 victimSteamId;
        std::string deathCause;
        std::string timestamp;
    };
    extern std::vector<DeathEvent> DeathEvents;

    struct TamedDinoKillEvent {
        std::string killerName;
        uint64 killerSteamId;
        std::string dinoName;
        std::string timestamp;
    };
    extern std::vector<TamedDinoKillEvent> TamedDinoKillEvents;

    struct PlayerSessionEvent {
        std::string playerName;
        uint64 steamId;
        std::string action;
        std::string timestamp;
    };
    extern std::vector<PlayerSessionEvent> PlayerSessionEvents;

    struct TribeEvent {
        std::string playerName;
        uint64 steamId;
        std::string tribeName;
        int tribeId;
        std::string action;
        std::string oldTribeName;
        std::string timestamp;
    };
    extern std::vector<TribeEvent> TribeEvents;

    struct BossEvent {
        std::string eventType;
        std::string bossName;
        std::string difficulty;
        std::string playerName;
        uint64 steamId;
        int participantCount;
        int durationSeconds;
        std::string timestamp;
    };
    extern std::vector<BossEvent> BossEvents;

    struct SpawnEvent {
        std::string spawnType;
        std::string type;
        float x;
        float y;
        float z;
        std::string timestamp;
    };
    extern std::vector<SpawnEvent> SpawnEvents;

    struct OSDTrackingInfo {
        AActor* actor = nullptr;
        FVector lastLocation;
        std::string typeStr;
        std::chrono::system_clock::time_point spawnTime;
        bool hasLanded = false;
        int stationaryCount = 0;
    };
    extern std::map<AActor*, OSDTrackingInfo> OSDTrackingMap;

    // Shared configuration
    extern bool EnableKillBroadcast;
    extern bool EnableDeathBroadcast;
    extern bool LogToFile;
    extern bool SendToApi;
    extern FString LogFilePath;
    extern std::string ApiUrl;
    extern std::string ApiKey;
    extern std::string ServerPass;
    extern bool EnableBossFightBroadcast;
    extern bool EnableOSDSpawnBroadcast;
    extern bool EnableElementalVeinSpawnBroadcast;
    extern bool EnablePlayerHitBroadcast;
    extern bool EnableClusterIdProtection;
    extern bool EnableSpoofing;
    extern std::string ClusterIdPassword;
    extern std::string SpoofedClusterId;
    extern FString RealClusterId;
    extern int DataSendInterval;
    extern bool TimerRunning;
    extern bool FromHarvest;

    // Shared constants
    const int HARVEST_CORRELATION_WINDOW_MS = 2000;
    const int BOSS_FIGHT_TIMEOUT_MS = 600000;

    // Shared utility functions
    FString GetCurrentTimestamp();
    void EnsureLogDirectoryExists();
    std::string GetCurrentMapName();
    std::string GetServerIPAndPort();
    int GetServerPopulation();
    uint64 GetSteamIDForPlayerID(int PlayerID);
    uint64 GetSteamIDFromCharacter(AShooterCharacter* Char, const bool GetFromControllerIfPossible = true);
    std::string GetBodyPartFromBone(const std::string& boneName);
    std::string GetTrackedResourceType(const std::string& itemName);
    bool ShouldIncludeInShotStats(const std::string& weaponName);
    bool IsHitscanWeapon(const std::string& weaponName);
    bool IsBolaWeapon(const std::string& weaponName);
    bool IsProjectileWeapon(const std::string& weaponName);

    // Event queue functions
    void QueueKillEvent(const std::string& killerName, uint64 killerSteamId,
                       const std::string& victimName, uint64 victimSteamId,
                       const std::string& weapon, const std::string& timestamp, bool onDino = false);
    void QueueDeathEvent(const std::string& victimName, uint64 victimSteamId,
                        const std::string& deathCause, const std::string& timestamp);
    void QueueTamedDinoKillEvent(const std::string& killerName, uint64 killerSteamId,
                                 const std::string& dinoName, const std::string& timestamp);
    void QueuePlayerSessionEvent(const std::string& playerName, uint64 steamId,
                                 const std::string& action, const std::string& timestamp);
    void QueueTribeEvent(const std::string& playerName, uint64 steamId,
                         const std::string& tribeName, int tribeId,
                         const std::string& action, const std::string& timestamp,
                         const std::string& oldTribeName = "");
    void QueueBossEvent(const std::string& eventType, const std::string& bossName,
                        const std::string& difficulty, const std::string& playerName,
                        uint64 steamId, int participantCount, int durationSeconds);
    void QueueSpawnEvent(const std::string& spawnType, const std::string& type, const FVector& location, const std::string& timestamp);

    // Logging functions
    void LogKillEvent(const FString& KillerName, uint64 KillerSteamId,
                      const FString& VictimName, uint64 VictimSteamId,
                      const FString& WeaponName);
    void LogDeathEvent(const FString& VictimName, uint64 VictimSteamId, const FString& DeathCause);
    void BroadcastKillMessage(const FString& KillerName, const FString& VictimName);
    void LoadConfig();

    // Hook function declarations
    // KillDeath hooks
    bool Hook_AShooterCharacter_Die(AShooterCharacter* _this, float KillingDamage, FDamageEvent* DamageEvent, AController* Killer, AActor* DamageCauser);
    bool Hook_APrimalDinoCharacter_Die(APrimalDinoCharacter* _this, float KillingDamage, FDamageEvent* DamageEvent, AController* Killer, AActor* DamageCauser);

    // PlayerSession hooks
    void Hook_AShooterGameMode_PostLogin(AShooterGameMode* _this, APlayerController* NewPlayer);
    void Hook_AShooterGameMode_Logout(AShooterGameMode* _this, AController* Exiting);
    unsigned __int64 Hook_AShooterGameMode_AddNewTribe(AShooterGameMode* _this, AShooterPlayerState* PlayerOwner, FString* TribeName, FTribeGovernment* TribeGovernment);
    bool Hook_AShooterPlayerState_AddToTribe(AShooterPlayerState* _this, FTribeData* MyNewTribe, bool bMergeTribe, bool bForce, bool bIsFromInvite, APlayerController* InviterPC);
    void Hook_AShooterPlayerState_ClearTribe(AShooterPlayerState* _this, bool bDontRemoveFromTribe, bool bForce, APlayerController* ForPC);
    void Hook_AShooterPlayerState_ServerRequestRenameTribe(AShooterPlayerState* _this, FString* NewTribeName);

    // DamageTracking hooks
    void Hook_AShooterWeapon_ServerStartFire(AShooterWeapon* _this);
    void Hook_AShooterWeapon_DealDamage(AShooterWeapon* _this, FHitResult* Impact, FVector* ShootDir, float DamageAmount, TSubclassOf<UDamageType> DamageType, float Impulse, AActor* DamageCauserOverride);
    float Hook_APrimalCharacter_TakeDamage(APrimalCharacter* _this, float Damage, FDamageEvent* DamageEvent, AController* EventInstigator, AActor* DamageCauser);

    // ResourceTracking hooks
    void Hook_UPrimalHarvestingComponent_DealDirectDamage(UPrimalHarvestingComponent* _this, APlayerController* ForPC, float DamageAmount, TSubclassOf<UDamageType> DamageTypeClass);
    void Hook_UPrimalInventoryComponent_NotifyItemAdded(UPrimalInventoryComponent* _this, UPrimalItem* theItem, bool bEquippedItem);
    void Hook_UPrimalHarvestingComponent_GiveHarvestResource(UPrimalHarvestingComponent* _this, UPrimalInventoryComponent* invComp, float AdditionalEffectiveness, TSubclassOf<UDamageType> DamageTypeClass, AActor* ToActor, TArray<FHarvestResourceEntry>* HarvestResourceEntryOverrides);
    void Hook_UAnimInstance_TriggerAnimNotifies(UAnimInstance* _this, float DeltaSeconds, __int64 bForceFireNotifies);
    int Hook_UPrimalInventoryComponent_IncrementItemTemplateQuantity(UPrimalInventoryComponent* _this, TSubclassOf<UPrimalItem> ItemTemplate, int amount, bool bReplicateToClient, bool bIsBlueprint, UPrimalItem** UseSpecificItem, UPrimalItem** IncrementedItem, bool bRequireExactClassMatch, bool bIsCraftingResourceConsumption, bool bIsFromUseConsumption, bool bIsArkTributeItem, bool ShowHUDNotification, bool bDontRecalcSpoilingTime, bool bDontExceedMaxItems);

    // BossTracking hooks
    bool Hook_AShooterGameState_StartMassTeleport(AShooterGameState* _this, FMassTeleportData* NewMassTeleportData, FTeleportDestination* TeleportDestination, AActor* InitiatingActor, TArray<AActor*> TeleportActors, TSubclassOf<APrimalBuff> BuffToApply, const float TeleportDuration, const float TeleportRadius, const bool bTeleportingSnapsToGround, const bool bMaintainRotation);

    // SpawnTracking hooks
    AActor* Hook_UWorld_SpawnActor(UWorld* world, UClass* clazz, FTransform const& transform, FActorSpawnParameters& params);
    void Hook_AActor_BeginPlay(AActor* _this);

    // ClusterID hooks
    void Hook_AShooterGameSession_FindSessions(AShooterGameSession* _this, TSharedPtr<FUniqueNetId, 0> UserId, FName SessionName, bool bIsLAN, bool bIsPresence, bool bRecreateSearchSettings, EListSessionStatus::Type FindType, bool bQueryNotFullSessions, bool bPasswordServers, const wchar_t* ServerName, FString ClusterId, FString AtlasId, FString ServerId, FString AuthListURL);

    // DECLARE_HOOK macros - these create inline variables for original function pointers
    // Placed here so all files can access them, including Main.cpp
    DECLARE_HOOK(AShooterCharacter_Die, bool, AShooterCharacter*, float, FDamageEvent*, AController*, AActor*);
    DECLARE_HOOK(APrimalDinoCharacter_Die, bool, APrimalDinoCharacter*, float, FDamageEvent*, AController*, AActor*);
    DECLARE_HOOK(AShooterGameMode_PostLogin, void, AShooterGameMode*, APlayerController*);
    DECLARE_HOOK(AShooterGameMode_Logout, void, AShooterGameMode*, AController*);
    DECLARE_HOOK(AShooterGameMode_AddNewTribe, unsigned __int64, AShooterGameMode*, AShooterPlayerState*, FString*, FTribeGovernment*);
    DECLARE_HOOK(AShooterPlayerState_AddToTribe, bool, AShooterPlayerState*, FTribeData*, bool, bool, bool, APlayerController*);
    DECLARE_HOOK(AShooterPlayerState_ClearTribe, void, AShooterPlayerState*, bool, bool, APlayerController*);
    DECLARE_HOOK(AShooterPlayerState_ServerRequestRenameTribe, void, AShooterPlayerState*, FString*);
    DECLARE_HOOK(AShooterWeapon_ServerStartFire, void, AShooterWeapon*);
    DECLARE_HOOK(AShooterWeapon_DealDamage, void, AShooterWeapon*, FHitResult*, FVector*, float, TSubclassOf<UDamageType>, float, AActor*);
    DECLARE_HOOK(APrimalCharacter_TakeDamage, float, APrimalCharacter*, float, FDamageEvent*, AController*, AActor*);
    DECLARE_HOOK(UPrimalHarvestingComponent_DealDirectDamage, void, UPrimalHarvestingComponent*, APlayerController*, float, TSubclassOf<UDamageType>);
    DECLARE_HOOK(UPrimalInventoryComponent_NotifyItemAdded, void, UPrimalInventoryComponent*, UPrimalItem*, bool);
    DECLARE_HOOK(UPrimalHarvestingComponent_GiveHarvestResource, void, UPrimalHarvestingComponent*, UPrimalInventoryComponent*, float, TSubclassOf<UDamageType>, AActor*, TArray<FHarvestResourceEntry>*);
    DECLARE_HOOK(UAnimInstance_TriggerAnimNotifies, void, ::UAnimInstance*, float, __int64);
    DECLARE_HOOK(UPrimalInventoryComponent_IncrementItemTemplateQuantity, int, UPrimalInventoryComponent*, TSubclassOf<UPrimalItem>, int, bool, bool, UPrimalItem**, UPrimalItem**, bool, bool, bool, bool, bool, bool, bool);
    DECLARE_HOOK(AShooterGameState_StartMassTeleport, bool, AShooterGameState*, FMassTeleportData*, FTeleportDestination*, AActor*, TArray<AActor*>, TSubclassOf<APrimalBuff>, const float, const float, const bool, const bool);
    DECLARE_HOOK(UWorld_SpawnActor, AActor*, UWorld*, UClass*, FTransform const&, FActorSpawnParameters&);
    DECLARE_HOOK(AActor_BeginPlay, void, AActor*);
    DECLARE_HOOK(AShooterGameSession_FindSessions, void, AShooterGameSession*, TSharedPtr<FUniqueNetId, 0>, FName, bool, bool, bool, EListSessionStatus::Type, bool, bool, const wchar_t*, FString, FString, FString, FString);

    // Timer functions
    void OnBatchedEventsTimer();
    void CheckOSDLanding();
    void CheckPendingShots();
    void ProcessProjectileBeginPlay(AActor* _this);
}
