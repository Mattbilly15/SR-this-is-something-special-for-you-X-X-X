#define _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING

#include "KillDeathTracker_Shared.h"
#include <map>
#include <vector>

namespace KillTracker
{
    // Shared data definitions
    std::map<uint64, ShotStats> PlayerShotStats;
    std::map<uint64, BolaStats> PlayerBolaStats;
    std::map<uint64, ResourceStats> PlayerResourceStats;
    std::map<uint64, BodyPartHitStats> PlayerBodyPartStats;
    std::vector<RecentShot> RecentShots;
    std::map<uint64, HarvestInfo> RecentHarvesters;
    std::map<uint64, BossFightInfo> PlayersInBossFight;
    std::vector<KillEvent> KillEvents;
    std::vector<DeathEvent> DeathEvents;
    std::vector<TamedDinoKillEvent> TamedDinoKillEvents;
    std::vector<PlayerSessionEvent> PlayerSessionEvents;
    std::vector<TribeEvent> TribeEvents;
    std::vector<BossEvent> BossEvents;
    std::vector<SpawnEvent> SpawnEvents;
    std::map<AActor*, OSDTrackingInfo> OSDTrackingMap;

    // Shared configuration
    bool EnableKillBroadcast = true;
    bool EnableDeathBroadcast = true;
    bool LogToFile = true;
    bool SendToApi = true;
    FString LogFilePath = "ArkApi/Plugins/KillTracker/Logs/";
    std::string ApiUrl = "https://www.bamsark.com/api/ark/event";
    std::string ApiKey = "bamsark_8f7k3m9x2p4n6q1w5t0v";
    std::string ServerPass = "pass";
    bool EnableBossFightBroadcast = true;
    bool EnableOSDSpawnBroadcast = true;
    bool EnableElementalVeinSpawnBroadcast = true;
    bool EnablePlayerHitBroadcast = true;
    bool EnableClusterIdProtection = false;
    bool EnableSpoofing = true;
    std::string ClusterIdPassword = "";
    std::string SpoofedClusterId = "FAKE_CLUSTER_ID";
    FString RealClusterId = "";
    int DataSendInterval = 10;
    bool TimerRunning = false;
    bool FromHarvest = false;
}
