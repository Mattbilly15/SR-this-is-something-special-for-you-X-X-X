#define _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING

#include "KillDeathTracker_Shared.h"
#include "json.hpp"
#include "Requests.h"
#include "Timer.h"
#include "API/ARK/Enums.h"
#include <fstream>
#include <algorithm>

namespace KillTracker
{
    // DECLARE_HOOK macros moved to KillDeathTracker_Shared.h

    bool Hook_AShooterGameState_StartMassTeleport(AShooterGameState* _this, FMassTeleportData* NewMassTeleportData,
                                                   FTeleportDestination* TeleportDestination, AActor* InitiatingActor,
                                                   TArray<AActor*> TeleportActors, TSubclassOf<APrimalBuff> BuffToApply,
                                                   const float TeleportDuration, const float TeleportRadius,
                                                   const bool bTeleportingSnapsToGround, const bool bMaintainRotation)
    {
        bool result = AShooterGameState_StartMassTeleport_original(_this, NewMassTeleportData, TeleportDestination,
                                                                   InitiatingActor, TeleportActors, BuffToApply,
                                                                   TeleportDuration, TeleportRadius,
                                                                   bTeleportingSnapsToGround, bMaintainRotation);

        if (!InitiatingActor)
            return result;

        FString InitiatorName;
        InitiatingActor->GetHumanReadableName(&InitiatorName);
        std::string initiatorStr = InitiatorName.ToString();
        std::transform(initiatorStr.begin(), initiatorStr.end(), initiatorStr.begin(), ::tolower);

        bool isBossTeleport = (initiatorStr.find("obelisk") != std::string::npos ||
                               initiatorStr.find("terminal") != std::string::npos ||
                               initiatorStr.find("boss") != std::string::npos ||
                               initiatorStr.find("tribute") != std::string::npos ||
                               initiatorStr.find("tek") != std::string::npos);

        if (!isBossTeleport)
            return result;

        FString Timestamp = GetCurrentTimestamp();

        int participantCount = 0;
        for (int i = 0; i < TeleportActors.Num(); i++)
        {
            AActor* Actor = TeleportActors[i];
            if (!Actor)
                continue;

            if (Actor->IsA(AShooterCharacter::GetPrivateStaticClass()))
            {
                AShooterCharacter* PlayerChar = static_cast<AShooterCharacter*>(Actor);
                AShooterPlayerController* PC = ArkApi::GetApiUtils().FindControllerFromCharacter(PlayerChar);

                if (PC)
                {
                    uint64 SteamId = ArkApi::IApiUtils::GetSteamIdFromController(PC);
                    FString PlayerName = ArkApi::IApiUtils::GetCharacterName(PC);

                    if (SteamId != 0)
                    {
                        BossFightInfo info;
                        info.startTime = std::chrono::system_clock::now();
                        info.bossName = PlayerName.ToString();
                        info.difficulty = "Unknown";
                        info.completed = false;

                        PlayersInBossFight[SteamId] = info;
                        participantCount++;

                        QueueBossEvent("boss_started", initiatorStr, "Unknown",
                                           PlayerName.ToString(), SteamId, participantCount, 0);
                    }
                }
            }
        }

        if (LogToFile && participantCount > 0)
        {
            EnsureLogDirectoryExists();
            std::string logPathStr = LogFilePath.ToString();
            std::string filePath = logPathStr + "boss_fights.log";
            std::ofstream file(filePath, std::ios::app);
            if (file.is_open())
            {
                file << "[" << Timestamp.ToString() << "] BOSS FIGHT STARTED: " << participantCount
                     << " players teleported via " << initiatorStr << "\n";
                file.close();
            }
        }

        return result;
    }
}
