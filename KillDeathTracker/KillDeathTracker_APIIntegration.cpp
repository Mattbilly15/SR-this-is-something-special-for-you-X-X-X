#define _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING

#include "KillDeathTracker_Shared.h"
#include "json.hpp"
#include "Requests.h"
#include "Timer.h"
#include "API/ARK/Enums.h"

namespace KillTracker
{
    void SendAllBatchedEventsToApi()
    {
        if (!SendToApi || ApiUrl.empty())
            return;

        bool hasEvents = !KillEvents.empty() || !DeathEvents.empty() ||
                        !TamedDinoKillEvents.empty() || !PlayerSessionEvents.empty() ||
                        !TribeEvents.empty() || !BossEvents.empty() || !SpawnEvents.empty() ||
                        !PlayerShotStats.empty() || !PlayerBolaStats.empty() || !PlayerResourceStats.empty() ||
                        !PlayerBodyPartStats.empty();

        std::string mapName = GetCurrentMapName();
        FString Timestamp = GetCurrentTimestamp();
        std::string timestampStr = Timestamp.ToString();

        nlohmann::json payload;
        payload["server_pass"] = ServerPass;
        payload["event_type"] = hasEvents ? "batch_events" : "heartbeat";
        payload["map"] = mapName;
        payload["timestamp"] = timestampStr;
        payload["batch_timestamp"] = timestampStr;
        payload["server_ip_port"] = GetServerIPAndPort();
        payload["server_population"] = GetServerPopulation();

        payload["kills"] = nlohmann::json::array();
        for (const auto& evt : KillEvents)
        {
            nlohmann::json kill;
            kill["killer_name"] = evt.killerName;
            kill["killer_steam_id"] = std::to_string(evt.killerSteamId);
            kill["victim_name"] = evt.victimName;
            kill["victim_steam_id"] = std::to_string(evt.victimSteamId);
            kill["weapon"] = evt.weaponName;
            kill["on_dino"] = evt.onDino;
            kill["timestamp"] = evt.timestamp;
            payload["kills"].push_back(kill);
        }

        payload["deaths"] = nlohmann::json::array();
        for (const auto& evt : DeathEvents)
        {
            nlohmann::json death;
            death["victim_name"] = evt.victimName;
            death["victim_steam_id"] = std::to_string(evt.victimSteamId);
            death["death_cause"] = evt.deathCause;
            death["timestamp"] = evt.timestamp;
            payload["deaths"].push_back(death);
        }

        payload["tamed_dino_kills"] = nlohmann::json::array();
        for (const auto& evt : TamedDinoKillEvents)
        {
            nlohmann::json dinoKill;
            dinoKill["killer_name"] = evt.killerName;
            dinoKill["killer_steam_id"] = std::to_string(evt.killerSteamId);
            dinoKill["dino_name"] = evt.dinoName;
            dinoKill["timestamp"] = evt.timestamp;
            payload["tamed_dino_kills"].push_back(dinoKill);
        }

        payload["player_sessions"] = nlohmann::json::array();
        for (const auto& evt : PlayerSessionEvents)
        {
            nlohmann::json session;
            session["player_name"] = evt.playerName;
            session["steam_id"] = std::to_string(evt.steamId);
            session["action"] = evt.action;
            session["timestamp"] = evt.timestamp;
            payload["player_sessions"].push_back(session);
        }

        payload["tribe_events"] = nlohmann::json::array();
        for (const auto& evt : TribeEvents)
        {
            nlohmann::json tribe;
            tribe["player_name"] = evt.playerName;
            tribe["steam_id"] = std::to_string(evt.steamId);
            tribe["tribe_name"] = evt.tribeName;
            tribe["tribe_id"] = evt.tribeId;
            tribe["action"] = evt.action;
            if (!evt.oldTribeName.empty())
                tribe["old_tribe_name"] = evt.oldTribeName;
            tribe["timestamp"] = evt.timestamp;
            payload["tribe_events"].push_back(tribe);
        }

        payload["boss_events"] = nlohmann::json::array();
        for (const auto& evt : BossEvents)
        {
            nlohmann::json boss;
            boss["event_type"] = evt.eventType;
            boss["boss_name"] = evt.bossName;
            boss["difficulty"] = evt.difficulty;
            boss["player_name"] = evt.playerName;
            boss["steam_id"] = std::to_string(evt.steamId);
            boss["participant_count"] = evt.participantCount;
            boss["duration_seconds"] = evt.durationSeconds;
            boss["timestamp"] = evt.timestamp;
            payload["boss_events"].push_back(boss);
        }

        payload["spawns"] = nlohmann::json::array();
        for (const auto& evt : SpawnEvents)
        {
            nlohmann::json spawn;
            spawn["spawn_type"] = evt.spawnType;
            spawn["type"] = evt.type;
            spawn["x"] = evt.x;
            spawn["y"] = evt.y;
            spawn["z"] = evt.z;
            spawn["timestamp"] = evt.timestamp;
            payload["spawns"].push_back(spawn);
        }

        payload["shot_stats"] = nlohmann::json::array();
        for (const auto& pair : PlayerShotStats)
        {
            uint64 steamId = pair.first;
            const ShotStats& stats = pair.second;

            if (stats.totalShots == 0)
                continue;

            if (!ShouldIncludeInShotStats(stats.lastWeaponUsed))
                continue;

            nlohmann::json playerStats;
            playerStats["player_name"] = stats.playerName;
            playerStats["steam_id"] = std::to_string(steamId);
            playerStats["total_shots"] = stats.totalShots;
            playerStats["player_hits"] = stats.playerHits;
            playerStats["dino_hits"] = stats.dinoHits;
            playerStats["damage_to_players"] = stats.totalDamageToPlayers;
            playerStats["last_weapon"] = stats.lastWeaponUsed;
            payload["shot_stats"].push_back(playerStats);
        }

        payload["bola_stats"] = nlohmann::json::array();
        for (const auto& pair : PlayerBolaStats)
        {
            uint64 steamId = pair.first;
            const BolaStats& stats = pair.second;

            if (stats.totalChucks == 0 && stats.playerHits == 0 && stats.dinoHits == 0)
                continue;

            nlohmann::json bolaStats;
            bolaStats["player_name"] = stats.playerName;
            bolaStats["steam_id"] = std::to_string(steamId);
            bolaStats["total_chucks"] = stats.totalChucks;
            bolaStats["player_hits"] = stats.playerHits;
            bolaStats["dino_hits"] = stats.dinoHits;
            bolaStats["total_hits"] = stats.playerHits + stats.dinoHits;
            bolaStats["bola_hits"] = stats.playerHits + stats.dinoHits;
            payload["bola_stats"].push_back(bolaStats);
        }

        payload["resource_stats"] = nlohmann::json::array();
        for (const auto& pair : PlayerResourceStats)
        {
            uint64 steamId = pair.first;
            const ResourceStats& stats = pair.second;

            if (stats.metal == 0 && stats.flint == 0 && stats.stone == 0 &&
                stats.pearl == 0 && stats.blackPearl == 0 && stats.oil == 0 &&
                stats.sulfur == 0 && stats.fiber == 0 && stats.crystal == 0 &&
                stats.wood == 0 && stats.thatch == 0 && stats.element == 0 &&
                stats.elementShards == 0 && stats.charcoal == 0 && stats.organicPoly == 0 &&
                stats.hide == 0 && stats.keratin == 0 && stats.chitin == 0 &&
                stats.megachelonShells == 0 && stats.obsidian == 0)
                continue;

            nlohmann::json resourceStats;
            resourceStats["player_name"] = stats.playerName;
            resourceStats["steam_id"] = std::to_string(steamId);
            resourceStats["metal"] = stats.metal;
            resourceStats["flint"] = stats.flint;
            resourceStats["stone"] = stats.stone;
            resourceStats["pearl"] = stats.pearl;
            resourceStats["black_pearl"] = stats.blackPearl;
            resourceStats["oil"] = stats.oil;
            resourceStats["sulfur"] = stats.sulfur;
            resourceStats["fiber"] = stats.fiber;
            resourceStats["crystal"] = stats.crystal;
            resourceStats["wood"] = stats.wood;
            resourceStats["thatch"] = stats.thatch;
            resourceStats["element"] = stats.element;
            resourceStats["element_shards"] = stats.elementShards;
            resourceStats["charcoal"] = stats.charcoal;
            resourceStats["organic_poly"] = stats.organicPoly;
            resourceStats["hide"] = stats.hide;
            resourceStats["keratin"] = stats.keratin;
            resourceStats["chitin"] = stats.chitin;
            resourceStats["megachelon_shells"] = stats.megachelonShells;
            resourceStats["obsidian"] = stats.obsidian;
            payload["resource_stats"].push_back(resourceStats);
        }

        payload["body_part_stats"] = nlohmann::json::array();
        for (const auto& pair : PlayerBodyPartStats)
        {
            uint64 steamId = pair.first;
            const BodyPartHitStats& stats = pair.second;

            int totalHits = stats.headHits + stats.neckHits + stats.chestHits +
                           stats.stomachHits + stats.armHits + stats.handHits +
                           stats.legHits + stats.footHits + stats.otherHits;

            if (totalHits == 0)
                continue;

            nlohmann::json bodyStats;
            bodyStats["player_name"] = stats.playerName;
            bodyStats["steam_id"] = std::to_string(steamId);
            bodyStats["head_hits"] = stats.headHits;
            bodyStats["neck_hits"] = stats.neckHits;
            bodyStats["chest_hits"] = stats.chestHits;
            bodyStats["stomach_hits"] = stats.stomachHits;
            bodyStats["arm_hits"] = stats.armHits;
            bodyStats["hand_hits"] = stats.handHits;
            bodyStats["leg_hits"] = stats.legHits;
            bodyStats["foot_hits"] = stats.footHits;
            bodyStats["other_hits"] = stats.otherHits;
            payload["body_part_stats"].push_back(bodyStats);
        }

        std::string jsonData = payload.dump();

        int totalEvents = static_cast<int>(KillEvents.size() + DeathEvents.size() + TamedDinoKillEvents.size() +
                         PlayerSessionEvents.size() + TribeEvents.size() + BossEvents.size() +
                         SpawnEvents.size() + payload["shot_stats"].size() + payload["bola_stats"].size() +
                         payload["resource_stats"].size() + payload["body_part_stats"].size());

        std::vector<std::string> headers = {
            "X-API-Key: " + ApiKey,
            "Authorization: Bearer " + ApiKey
        };

        API::Requests::Get().CreatePostRequest(
            ApiUrl,
            [totalEvents](bool success, std::string response) {
                if (!success)
                {
                    Log::GetLog()->warn("KillTracker: Batch API request failed: {}", response);
                }
            },
            jsonData,
            "application/json",
            headers
        );

        KillEvents.clear();
        DeathEvents.clear();
        TamedDinoKillEvents.clear();
        PlayerSessionEvents.clear();
        TribeEvents.clear();
        BossEvents.clear();
        SpawnEvents.clear();
        PlayerShotStats.clear();
        PlayerBolaStats.clear();
        PlayerResourceStats.clear();
        PlayerBodyPartStats.clear();
    }

    void OnBatchedEventsTimer()
    {
        SendAllBatchedEventsToApi();
    }

    void SendAllBodyPartStatsToApi()
    {
        if (!SendToApi || ApiUrl.empty())
            return;

        if (PlayerBodyPartStats.empty())
            return;

        std::string mapName = GetCurrentMapName();
        FString Timestamp = GetCurrentTimestamp();
        std::string timestampStr = Timestamp.ToString();

        nlohmann::json payload;
        payload["event_type"] = "body_part_hits_batch";
        payload["map"] = mapName;
        payload["timestamp"] = timestampStr;
        payload["players"] = nlohmann::json::array();

        for (const auto& pair : PlayerBodyPartStats)
        {
            uint64 steamId = pair.first;
            const BodyPartHitStats& stats = pair.second;

            int totalHits = stats.headHits + stats.neckHits + stats.chestHits + stats.stomachHits +
                            stats.armHits + stats.handHits + stats.legHits + stats.footHits + stats.otherHits;
            if (totalHits == 0)
                continue;

            nlohmann::json playerStats;
            playerStats["player_name"] = stats.playerName;
            playerStats["steam_id"] = std::to_string(steamId);

            playerStats["head_hits"] = stats.headHits;
            playerStats["neck_hits"] = stats.neckHits;
            playerStats["chest_hits"] = stats.chestHits;
            playerStats["stomach_hits"] = stats.stomachHits;
            playerStats["arm_hits"] = stats.armHits;
            playerStats["hand_hits"] = stats.handHits;
            playerStats["leg_hits"] = stats.legHits;
            playerStats["foot_hits"] = stats.footHits;
            playerStats["other_hits"] = stats.otherHits;
            playerStats["total_hits"] = totalHits;

            float headshotPct = totalHits > 0 ? (static_cast<float>(stats.headHits) / totalHits) * 100.0f : 0.0f;
            playerStats["headshot_percent"] = headshotPct;

            payload["players"].push_back(playerStats);
        }

        if (payload["players"].empty())
            return;

        std::string jsonData = payload.dump();

        std::vector<std::string> headers = {
            "X-API-Key: " + ApiKey,
            "Authorization: Bearer " + ApiKey
        };

        API::Requests::Get().CreatePostRequest(
            ApiUrl,
            [](bool success, std::string response) {
                if (success)
                {
                }
                else
                {
                    Log::GetLog()->warn("KillTracker: Body part stats API request failed: {}", response);
                }
            },
            jsonData,
            "application/json",
            headers
        );

        PlayerBodyPartStats.clear();
    }
}
