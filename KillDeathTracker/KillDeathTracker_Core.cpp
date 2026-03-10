#define _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING

#include "KillDeathTracker_Shared.h"
#include "json.hpp"
#include "Requests.h"
#include "Timer.h"
#include "API/ARK/Enums.h"
#include <fstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <Windows.h>

namespace KillTracker
{
    struct FAnimNotifyEvent
    {
        FName NotifyNameField() { return *GetNativePointerField<FName*>(this, "FAnimNotifyEvent.NotifyName"); }
    };

    struct UAnimInstance : UObject
    {
        TArray<FAnimNotifyEvent*> AnimNotifiesField() { return *GetNativePointerField<TArray<FAnimNotifyEvent*>*>(this, "UAnimInstance.AnimNotifies"); }
    };

    FString GetCurrentTimestamp()
    {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);

        std::tm tm_now;
        localtime_s(&tm_now, &time_t_now);

        std::ostringstream oss;
        oss << std::put_time(&tm_now, "%Y-%m-%d %H:%M:%S");

        return FString(oss.str().c_str());
    }

    void EnsureLogDirectoryExists()
    {
        try
        {
            std::string logPath = LogFilePath.ToString();
            std::filesystem::create_directories(logPath);
        }
        catch (const std::exception& e)
        {
            Log::GetLog()->error("KillTracker: Failed to create log directory: {}", e.what());
        }
    }

    std::string GetCurrentMapName()
    {
        try
        {
            AShooterGameMode* GameMode = ArkApi::GetApiUtils().GetShooterGameMode();
            if (GameMode)
            {
                FString MapName;
                GameMode->GetMapName(&MapName);
                return MapName.ToString();
            }
        }
        catch (...)
        {
            Log::GetLog()->error("KillTracker: Failed to get map name");
        }
        return "Unknown";
    }

    std::string GetServerIPAndPort()
    {
        try
        {
            LPWSTR cmdLineW = GetCommandLineW();
            if (cmdLineW)
            {
                int size_needed = WideCharToMultiByte(CP_UTF8, 0, cmdLineW, -1, NULL, 0, NULL, NULL);
                if (size_needed > 0)
                {
                    std::string cmdLineStr(size_needed, 0);
                    WideCharToMultiByte(CP_UTF8, 0, cmdLineW, -1, &cmdLineStr[0], size_needed, NULL, NULL);
                    cmdLineStr.resize(size_needed - 1);

                    std::string serverIP = "0.0.0.0";
                    std::string queryPort = "27015";

                    size_t multiHomePos = cmdLineStr.find("MultiHome=");
                    if (multiHomePos != std::string::npos)
                    {
                        size_t start = multiHomePos + 10;
                        size_t end = cmdLineStr.find('?', start);
                        if (end == std::string::npos)
                            end = cmdLineStr.find(' ', start);
                        if (end == std::string::npos)
                            end = cmdLineStr.length();

                        serverIP = cmdLineStr.substr(start, end - start);
                    }

                    size_t queryPortPos = cmdLineStr.find("QueryPort=");
                    if (queryPortPos != std::string::npos)
                    {
                        size_t start = queryPortPos + 10;
                        size_t end = cmdLineStr.find('?', start);
                        if (end == std::string::npos)
                            end = cmdLineStr.find(' ', start);
                        if (end == std::string::npos)
                            end = cmdLineStr.length();

                        queryPort = cmdLineStr.substr(start, end - start);
                    }

                    std::string result = serverIP + ":" + queryPort;
                    return result;
                }
            }
        }
        catch (const std::exception& e)
        {
            Log::GetLog()->error("KillTracker: Exception getting server IP and port: {}", e.what());
        }
        catch (...)
        {
            Log::GetLog()->error("KillTracker: Failed to get server IP and port");
        }

        return "0.0.0.0:27015";
    }

    int GetServerPopulation()
    {
        try
        {
            AShooterGameState* GameState = ArkApi::GetApiUtils().GetGameState();
            if (GameState)
            {
                return GameState->NumPlayerConnectedField();
            }

            AShooterGameMode* GameMode = ArkApi::GetApiUtils().GetShooterGameMode();
            if (GameMode)
            {
                return GameMode->GetNumPlayers();
            }
        }
        catch (...)
        {
            Log::GetLog()->error("KillTracker: Failed to get server population");
        }

        return 0;
    }

    uint64 GetSteamIDForPlayerID(int PlayerID)
    {
        uint64 SteamID = NULL;

        if (PlayerID == 0)
        {
            return SteamID;
        }

        SteamID = ArkApi::GetApiUtils().GetShooterGameMode()->GetSteamIDForPlayerID(PlayerID);
        if (SteamID == NULL)
        {
            const auto& PCS = ArkApi::GetApiUtils().GetWorld()->PlayerControllerListField();
            for (TWeakObjectPtr<APlayerController> APC : PCS)
            {
                AShooterPlayerController* PC = static_cast<AShooterPlayerController*>(APC.Get());
                if (PC && PC->LinkedPlayerIDField() == PlayerID)
                {
                    SteamID = ArkApi::GetApiUtils().GetSteamIdFromController(PC);
                    break;
                }
            }
        }

        return SteamID;
    }

    uint64 GetSteamIDFromCharacter(AShooterCharacter* Char, const bool GetFromControllerIfPossible)
    {
        uint64 SteamID = NULL;
        if (Char)
        {
            if (Char->GetPlayerData())
            {
                FString FSteamID;
                Char->GetPlayerData()->GetUniqueIdString(&FSteamID);
                SteamID = FCString::Atoi64(*FSteamID);
            }
            else if (uint64 ID = GetSteamIDForPlayerID(static_cast<int>(Char->LinkedPlayerDataIDField())); ID != 0)
                SteamID = ID;
            else if (AShooterPlayerController* PC = ArkApi::GetApiUtils().FindControllerFromCharacter(Char); PC && GetFromControllerIfPossible)
                SteamID = ArkApi::GetApiUtils().GetSteamIdFromController(PC);
        }

        return SteamID;
    }

    std::string GetBodyPartFromBone(const std::string& boneName)
    {
        std::string lowerBone = boneName;
        std::transform(lowerBone.begin(), lowerBone.end(), lowerBone.begin(), ::tolower);

        if (lowerBone.find("head") != std::string::npos ||
            lowerBone.find("skull") != std::string::npos ||
            lowerBone.find("jaw") != std::string::npos ||
            lowerBone.find("face") != std::string::npos)
            return "head";

        if (lowerBone.find("neck") != std::string::npos)
            return "neck";

        if (lowerBone.find("chest") != std::string::npos ||
            lowerBone.find("spine") != std::string::npos ||
            lowerBone.find("shoulder") != std::string::npos ||
            lowerBone.find("clavicle") != std::string::npos ||
            lowerBone.find("torso") != std::string::npos ||
            lowerBone.find("upperback") != std::string::npos)
            return "chest";

        if (lowerBone.find("pelvis") != std::string::npos ||
            lowerBone.find("stomach") != std::string::npos ||
            lowerBone.find("abdomen") != std::string::npos ||
            lowerBone.find("hip") != std::string::npos ||
            lowerBone.find("lowerback") != std::string::npos)
            return "stomach";

        if (lowerBone.find("upperarm") != std::string::npos ||
            lowerBone.find("lowerarm") != std::string::npos ||
            lowerBone.find("forearm") != std::string::npos ||
            lowerBone.find("arm") != std::string::npos ||
            lowerBone.find("elbow") != std::string::npos)
            return "arm";

        if (lowerBone.find("hand") != std::string::npos ||
            lowerBone.find("finger") != std::string::npos ||
            lowerBone.find("wrist") != std::string::npos ||
            lowerBone.find("thumb") != std::string::npos)
            return "hand";

        if (lowerBone.find("thigh") != std::string::npos ||
            lowerBone.find("calf") != std::string::npos ||
            lowerBone.find("leg") != std::string::npos ||
            lowerBone.find("knee") != std::string::npos ||
            lowerBone.find("shin") != std::string::npos)
            return "leg";

        if (lowerBone.find("foot") != std::string::npos ||
            lowerBone.find("toe") != std::string::npos ||
            lowerBone.find("ankle") != std::string::npos ||
            lowerBone.find("heel") != std::string::npos)
            return "foot";

        return "other";
    }

    std::string GetTrackedResourceType(const std::string& itemName)
    {
        std::string lowerName = itemName;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

        if (lowerName.find("metal") != std::string::npos && lowerName.find("ingot") == std::string::npos)
            return "metal";
        if (lowerName.find("flint") != std::string::npos)
            return "flint";
        if (lowerName.find("stone") != std::string::npos && lowerName.find("cementing") == std::string::npos)
            return "stone";
        if (lowerName.find("black pearl") != std::string::npos || lowerName.find("blackpearl") != std::string::npos)
            return "black_pearl";
        if (lowerName.find("silica pearl") != std::string::npos || (lowerName.find("pearl") != std::string::npos && lowerName.find("black") == std::string::npos))
            return "pearl";
        if (lowerName.find("oil") != std::string::npos && lowerName.find("gasoline") == std::string::npos)
            return "oil";
        if (lowerName.find("sulfur") != std::string::npos)
            return "sulfur";
        if (lowerName.find("fiber") != std::string::npos)
            return "fiber";
        if (lowerName.find("crystal") != std::string::npos && lowerName.find("element") == std::string::npos)
            return "crystal";
        if (lowerName.find("wood") != std::string::npos)
            return "wood";
        if (lowerName.find("thatch") != std::string::npos)
            return "thatch";
        if (lowerName.find("element shard") != std::string::npos || lowerName.find("elementshard") != std::string::npos)
            return "element_shards";
        if (lowerName.find("element") != std::string::npos)
            return "element";
        if (lowerName.find("charcoal") != std::string::npos)
            return "charcoal";
        if (lowerName.find("organic polymer") != std::string::npos || lowerName.find("organicpoly") != std::string::npos)
            return "organic_poly";
        if (lowerName.find("hide") != std::string::npos)
            return "hide";
        if (lowerName.find("keratin") != std::string::npos)
            return "keratin";
        if (lowerName.find("chitin") != std::string::npos)
            return "chitin";
        if (lowerName.find("megachelon shell") != std::string::npos || lowerName.find("megachelonshell") != std::string::npos)
            return "megachelon_shells";
        if (lowerName.find("obsidian") != std::string::npos)
            return "obsidian";

        return "";
    }

    bool IsHitscanWeapon(const std::string& weaponName)
    {
        std::string lowerName = weaponName;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

        bool isHitscan = (lowerName.find("fabi") != std::string::npos ||
                         lowerName.find("pump shotgun") != std::string::npos ||
                         lowerName.find("sniper rifle") != std::string::npos ||
                         lowerName.find("fabricated sniper") != std::string::npos);

        return isHitscan;
    }

    bool IsBolaWeapon(const std::string& weaponName)
    {
        std::string lowerName = weaponName;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

        return (lowerName.find("bola") != std::string::npos ||
                lowerName.find("net") != std::string::npos);
    }

    bool IsProjectileWeapon(const std::string& weaponName)
    {
        std::string lowerName = weaponName;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

        return (lowerName.find("compound bow") != std::string::npos ||
                lowerName.find("bola") != std::string::npos ||
                lowerName.find("net") != std::string::npos);
    }

    bool ShouldIncludeInShotStats(const std::string& weaponName)
    {
        std::string lowerName = weaponName;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

        return (lowerName.find("sniper rifle") != std::string::npos ||
                lowerName.find("fabricated sniper") != std::string::npos ||
                lowerName.find("pump shotgun") != std::string::npos ||
                lowerName.find("compound bow") != std::string::npos);
    }

    void QueueKillEvent(const std::string& killerName, uint64 killerSteamId,
                       const std::string& victimName, uint64 victimSteamId,
                       const std::string& weapon, const std::string& timestamp, bool onDino)
    {
        KillEvent evt;
        evt.killerName = killerName;
        evt.killerSteamId = killerSteamId;
        evt.victimName = victimName;
        evt.victimSteamId = victimSteamId;
        evt.weaponName = weapon;
        evt.timestamp = timestamp;
        evt.onDino = onDino;
        KillEvents.push_back(evt);
    }

    void QueueDeathEvent(const std::string& victimName, uint64 victimSteamId,
                        const std::string& deathCause, const std::string& timestamp)
    {
        DeathEvent evt;
        evt.victimName = victimName;
        evt.victimSteamId = victimSteamId;
        evt.deathCause = deathCause;
        evt.timestamp = timestamp;
        DeathEvents.push_back(evt);
    }

    void QueueTamedDinoKillEvent(const std::string& killerName, uint64 killerSteamId,
                                 const std::string& dinoName, const std::string& timestamp)
    {
        TamedDinoKillEvent evt;
        evt.killerName = killerName;
        evt.killerSteamId = killerSteamId;
        evt.dinoName = dinoName;
        evt.timestamp = timestamp;
        TamedDinoKillEvents.push_back(evt);
    }

    void QueuePlayerSessionEvent(const std::string& playerName, uint64 steamId,
                                 const std::string& action, const std::string& timestamp)
    {
        PlayerSessionEvent evt;
        evt.playerName = playerName;
        evt.steamId = steamId;
        evt.action = action;
        evt.timestamp = timestamp;
        PlayerSessionEvents.push_back(evt);
    }

    void QueueTribeEvent(const std::string& playerName, uint64 steamId,
                         const std::string& tribeName, int tribeId,
                         const std::string& action, const std::string& timestamp,
                         const std::string& oldTribeName)
    {
        TribeEvent evt;
        evt.playerName = playerName;
        evt.steamId = steamId;
        evt.tribeName = tribeName;
        evt.tribeId = tribeId;
        evt.action = action;
        evt.oldTribeName = oldTribeName;
        evt.timestamp = timestamp;
        TribeEvents.push_back(evt);
    }

    void QueueBossEvent(const std::string& eventType, const std::string& bossName,
                        const std::string& difficulty, const std::string& playerName,
                        uint64 steamId, int participantCount, int durationSeconds)
    {
        FString Timestamp = GetCurrentTimestamp();
        std::string timestampStr = Timestamp.ToString();

        BossEvent evt;
        evt.eventType = eventType;
        evt.bossName = bossName;
        evt.difficulty = difficulty;
        evt.playerName = playerName;
        evt.steamId = steamId;
        evt.participantCount = participantCount;
        evt.durationSeconds = durationSeconds;
        evt.timestamp = timestampStr;
        BossEvents.push_back(evt);
    }

    void QueueSpawnEvent(const std::string& spawnType, const std::string& type, const FVector& location, const std::string& timestamp)
    {
        try
        {
            if (ArkApi::GetApiUtils().GetStatus() != ArkApi::ServerStatus::Ready)
            {
                Log::GetLog()->warn("KillTracker: Server not ready, skipping coordinate conversion");
                return;
            }

            ArkApi::MapCoords coords = ArkApi::GetApiUtils().FVectorToCoords(location);

            SpawnEvent evt;
            evt.spawnType = spawnType;
            evt.type = type;
            evt.x = coords.x;
            evt.y = coords.y;
            evt.z = 0.0f;
            evt.timestamp = timestamp;
            SpawnEvents.push_back(evt);
        }
        catch (...)
        {
            Log::GetLog()->error("KillTracker: Exception in QueueSpawnEvent, skipping event");
        }
    }

    void LogKillEvent(const FString& KillerName, uint64 KillerSteamId,
                      const FString& VictimName, uint64 VictimSteamId,
                      const FString& WeaponName)
    {
        FString Timestamp = GetCurrentTimestamp();

        std::string killerNameStr = KillerName.ToString();
        std::string victimNameStr = VictimName.ToString();
        std::string weaponNameStr = WeaponName.ToString();
        std::string timestampStr = Timestamp.ToString();

        QueueKillEvent(killerNameStr, KillerSteamId, victimNameStr, VictimSteamId, weaponNameStr, timestampStr, false);

        if (LogToFile)
        {
            EnsureLogDirectoryExists();

            std::string logPathStr = LogFilePath.ToString();
            std::string filePath = logPathStr + "kills.log";
            std::ofstream file(filePath, std::ios::app);
            if (file.is_open())
            {
                file << "[" << timestampStr << "] KILL: " << killerNameStr
                     << " (SteamID: " << KillerSteamId << ") killed "
                     << victimNameStr << " (SteamID: " << VictimSteamId
                     << ") with " << weaponNameStr << "\n";
                file.close();
            }

            std::string combinedFilePath = logPathStr + "all_events.log";
            std::ofstream combinedFile(combinedFilePath, std::ios::app);
            if (combinedFile.is_open())
            {
                combinedFile << "[" << timestampStr << "] KILL: " << killerNameStr
                             << " (SteamID: " << KillerSteamId << ") killed "
                             << victimNameStr << " (SteamID: " << VictimSteamId
                             << ") with " << weaponNameStr << "\n";
                combinedFile.close();
            }
        }
    }

    void LogDeathEvent(const FString& VictimName, uint64 VictimSteamId, const FString& DeathCause)
    {
        FString Timestamp = GetCurrentTimestamp();

        std::string victimNameStr = VictimName.ToString();
        std::string deathCauseStr = DeathCause.ToString();
        std::string timestampStr = Timestamp.ToString();

        QueueDeathEvent(victimNameStr, VictimSteamId, deathCauseStr, timestampStr);

        if (LogToFile)
        {
            EnsureLogDirectoryExists();

            std::string logPathStr = LogFilePath.ToString();
            std::string filePath = logPathStr + "deaths.log";
            std::ofstream file(filePath, std::ios::app);
            if (file.is_open())
            {
                file << "[" << timestampStr << "] DEATH: " << victimNameStr
                     << " (SteamID: " << VictimSteamId << ") died from "
                     << deathCauseStr << "\n";
                file.close();
            }

            std::string combinedFilePath = logPathStr + "all_events.log";
            std::ofstream combinedFile(combinedFilePath, std::ios::app);
            if (combinedFile.is_open())
            {
                combinedFile << "[" << timestampStr << "] DEATH: " << victimNameStr
                             << " (SteamID: " << VictimSteamId << ") died from "
                             << deathCauseStr << "\n";
                combinedFile.close();
            }
        }
    }

    void BroadcastKillMessage(const FString& KillerName, const FString& VictimName)
    {
    }

    void LoadConfig()
    {
        const std::string ConfigPath = ArkApi::Tools::GetCurrentDir() + "/ArkApi/Plugins/KillTracker/config.json";

        std::ifstream file(ConfigPath);
        if (!file.is_open())
        {
            Log::GetLog()->warn("KillTracker: Config file not found, using defaults");
            return;
        }

        nlohmann::json config;
        try
        {
            file >> config;

            if (config.contains("Broadcasts"))
            {
                const auto& broadcasts = config["Broadcasts"];
                EnableKillBroadcast = broadcasts.value("EnableKillBroadcast", true);
                EnableDeathBroadcast = broadcasts.value("EnableDeathBroadcast", true);
                EnableBossFightBroadcast = broadcasts.value("EnableBossFightBroadcast", true);
                EnableOSDSpawnBroadcast = broadcasts.value("EnableOSDSpawnBroadcast", true);
                EnableElementalVeinSpawnBroadcast = broadcasts.value("EnableElementalVeinSpawnBroadcast", true);
                EnablePlayerHitBroadcast = broadcasts.value("EnablePlayerHitBroadcast", true);
            }
            else
            {
                EnableKillBroadcast = config.value("EnableKillBroadcast", true);
                EnableDeathBroadcast = config.value("EnableDeathBroadcast", true);
                EnableBossFightBroadcast = config.value("EnableBossFightBroadcast", true);
                EnableOSDSpawnBroadcast = config.value("EnableOSDSpawnBroadcast", true);
                EnableElementalVeinSpawnBroadcast = config.value("EnableElementalVeinSpawnBroadcast", true);
                EnablePlayerHitBroadcast = config.value("EnablePlayerHitBroadcast", true);
            }

            if (config.contains("Logging"))
            {
                const auto& logging = config["Logging"];
                LogToFile = logging.value("LogToFile", true);
                std::string logPath = logging.value("LogFilePath", "ArkApi/Plugins/KillTracker/Logs/");
                LogFilePath = FString(logPath.c_str());
            }
            else
            {
                LogToFile = config.value("LogToFile", true);
                std::string logPath = config.value("LogFilePath", "ArkApi/Plugins/KillTracker/Logs/");
                LogFilePath = FString(logPath.c_str());
            }

            if (config.contains("API"))
            {
                const auto& api = config["API"];
                SendToApi = api.value("SendToApi", true);
                ApiUrl = api.value("ApiUrl", "https://www.bamsark.com/api/ark/event");
                ApiKey = api.value("ApiKey", "bamsark_8f7k3m9x2p4n6q1w5t0v");
                ServerPass = api.value("ServerPass", "pass");
                DataSendInterval = api.value("DataSendInterval", 10);
            }
            else
            {
                SendToApi = config.value("SendToApi", true);
                ApiUrl = config.value("ApiUrl", "https://www.bamsark.com/api/ark/event");
                ApiKey = config.value("ApiKey", "bamsark_8f7k3m9x2p4n6q1w5t0v");
                ServerPass = config.value("ServerPass", "pass");
                DataSendInterval = config.value("DataSendInterval", 10);
            }

            if (config.contains("ClusterIdProtection"))
            {
                const auto& clusterId = config["ClusterIdProtection"];
                EnableClusterIdProtection = clusterId.value("EnableClusterIdProtection", false);
                EnableSpoofing = clusterId.value("EnableSpoofing", true);
                ClusterIdPassword = clusterId.value("ClusterIdPassword", "");
                SpoofedClusterId = clusterId.value("SpoofedClusterId", "FAKE_CLUSTER_ID");
            }
            else
            {
                EnableClusterIdProtection = config.value("EnableClusterIdProtection", false);
                EnableSpoofing = config.value("EnableSpoofing", true);
                ClusterIdPassword = config.value("ClusterIdPassword", "");
                SpoofedClusterId = config.value("SpoofedClusterId", "FAKE_CLUSTER_ID");
            }

            if (EnableClusterIdProtection)
            {
                if (ClusterIdPassword.empty())
                {
                    Log::GetLog()->warn("KillTracker: WARNING - Cluster ID protection enabled but no password set!");
                }
            }
        }
        catch (const std::exception& e)
        {
            Log::GetLog()->error("KillTracker: Failed to parse config: {}", e.what());
        }

        file.close();
    }
}
