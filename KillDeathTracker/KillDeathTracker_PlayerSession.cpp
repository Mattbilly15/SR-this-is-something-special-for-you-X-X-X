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

    void Hook_AShooterGameMode_PostLogin(AShooterGameMode* _this, APlayerController* NewPlayer)
    {
        AShooterGameMode_PostLogin_original(_this, NewPlayer);

        if (!NewPlayer)
            return;

        AShooterPlayerController* ShooterPlayer = static_cast<AShooterPlayerController*>(NewPlayer);
        if (!ShooterPlayer)
            return;

        uint64 SteamId = ArkApi::IApiUtils::GetSteamIdFromController(ShooterPlayer);
        FString PlayerName = ArkApi::IApiUtils::GetCharacterName(ShooterPlayer);

        if (SteamId == 0)
            return;

        FString Timestamp = GetCurrentTimestamp();
        std::string playerNameStr = PlayerName.ToString();
        std::string timestampStr = Timestamp.ToString();

        QueuePlayerSessionEvent(playerNameStr, SteamId, "join", timestampStr);

        if (LogToFile)
        {
            EnsureLogDirectoryExists();

            std::string logPathStr = LogFilePath.ToString();
            std::string filePath = logPathStr + "sessions.log";
            std::ofstream file(filePath, std::ios::app);
            if (file.is_open())
            {
                file << "[" << timestampStr << "] JOIN: " << playerNameStr
                     << " (SteamID: " << SteamId << ")\n";
                file.close();
            }
        }
    }

    void Hook_AShooterGameMode_Logout(AShooterGameMode* _this, AController* Exiting)
    {
        uint64 SteamId = 0;
        FString PlayerName = TEXT("Unknown");

        if (Exiting)
        {
            AShooterPlayerController* ShooterPlayer = static_cast<AShooterPlayerController*>(Exiting);
            if (ShooterPlayer)
            {
                SteamId = ArkApi::IApiUtils::GetSteamIdFromController(ShooterPlayer);
                PlayerName = ArkApi::IApiUtils::GetCharacterName(ShooterPlayer);
            }
        }

        AShooterGameMode_Logout_original(_this, Exiting);

        if (SteamId == 0)
            return;

        FString Timestamp = GetCurrentTimestamp();
        std::string playerNameStr = PlayerName.ToString();
        std::string timestampStr = Timestamp.ToString();

        QueuePlayerSessionEvent(playerNameStr, SteamId, "leave", timestampStr);

        if (LogToFile)
        {
            EnsureLogDirectoryExists();

            std::string logPathStr = LogFilePath.ToString();
            std::string filePath = logPathStr + "sessions.log";
            std::ofstream file(filePath, std::ios::app);
            if (file.is_open())
            {
                file << "[" << timestampStr << "] LEAVE: " << playerNameStr
                     << " (SteamID: " << SteamId << ")\n";
                file.close();
            }
        }
    }

    unsigned __int64 Hook_AShooterGameMode_AddNewTribe(AShooterGameMode* _this, AShooterPlayerState* PlayerOwner,
                                                        FString* TribeName, FTribeGovernment* TribeGovernment)
    {
        unsigned __int64 TribeId = AShooterGameMode_AddNewTribe_original(_this, PlayerOwner, TribeName, TribeGovernment);

        FString PlayerName = TEXT("Unknown");
        uint64 SteamId = 0;
        FString TribeNameStr = TribeName ? *TribeName : TEXT("Unknown Tribe");

        if (PlayerOwner)
        {
            PlayerName = PlayerOwner->PlayerNameField();

            FString UniqueIdStr;
            PlayerOwner->GetUniqueIdString(&UniqueIdStr);
            if (!UniqueIdStr.IsEmpty())
            {
                try {
                    SteamId = std::stoull(UniqueIdStr.ToString());
                } catch (...) {
                    SteamId = 0;
                }
            }
        }

        FString Timestamp = GetCurrentTimestamp();
        std::string playerNameStr = PlayerName.ToString();
        std::string tribeNameStdStr = TribeNameStr.ToString();
        std::string timestampStr = Timestamp.ToString();

        QueueTribeEvent(playerNameStr, SteamId, tribeNameStdStr, static_cast<int>(TribeId), "create", timestampStr);

        if (LogToFile)
        {
            EnsureLogDirectoryExists();

            std::string logPathStr = LogFilePath.ToString();
            std::string filePath = logPathStr + "tribes.log";
            std::ofstream file(filePath, std::ios::app);
            if (file.is_open())
            {
                file << "[" << timestampStr << "] TRIBE CREATED: " << playerNameStr
                     << " (SteamID: " << SteamId << ") created tribe '"
                     << tribeNameStdStr << "' (ID: " << TribeId << ")\n";
                file.close();
            }
        }

        return TribeId;
    }

    bool Hook_AShooterPlayerState_AddToTribe(AShooterPlayerState* _this, FTribeData* MyNewTribe,
                                              bool bMergeTribe, bool bForce, bool bIsFromInvite,
                                              APlayerController* InviterPC)
    {
        FString PlayerName = TEXT("Unknown");
        uint64 SteamId = 0;
        FString TribeNameStr = TEXT("Unknown Tribe");
        int TribeId = 0;

        if (_this)
        {
            PlayerName = _this->PlayerNameField();

            FString UniqueIdStr;
            _this->GetUniqueIdString(&UniqueIdStr);
            if (!UniqueIdStr.IsEmpty())
            {
                try {
                    SteamId = std::stoull(UniqueIdStr.ToString());
                } catch (...) {
                    SteamId = 0;
                }
            }
        }

        if (MyNewTribe)
        {
            TribeNameStr = MyNewTribe->TribeNameField();
            TribeId = MyNewTribe->TribeIDField();
        }

        bool result = AShooterPlayerState_AddToTribe_original(_this, MyNewTribe, bMergeTribe, bForce, bIsFromInvite, InviterPC);

        if (result)
        {
            FString Timestamp = GetCurrentTimestamp();
            std::string playerNameStr = PlayerName.ToString();
            std::string tribeNameStdStr = TribeNameStr.ToString();
            std::string timestampStr = Timestamp.ToString();

            QueueTribeEvent(playerNameStr, SteamId, tribeNameStdStr, TribeId, "join", timestampStr);

            if (LogToFile)
            {
                EnsureLogDirectoryExists();

                std::string logPathStr = LogFilePath.ToString();
                std::string filePath = logPathStr + "tribes.log";
                std::ofstream file(filePath, std::ios::app);
                if (file.is_open())
                {
                    file << "[" << timestampStr << "] TRIBE JOINED: " << playerNameStr
                         << " (SteamID: " << SteamId << ") joined tribe '"
                         << tribeNameStdStr << "' (ID: " << TribeId << ")\n";
                    file.close();
                }
            }
        }

        return result;
    }

    void Hook_AShooterPlayerState_ClearTribe(AShooterPlayerState* _this, bool bDontRemoveFromTribe,
                                              bool bForce, APlayerController* ForPC)
    {
        FString PlayerName = TEXT("Unknown");
        uint64 SteamId = 0;
        FString TribeNameStr = TEXT("Unknown Tribe");
        int TribeId = 0;

        if (_this)
        {
            PlayerName = _this->PlayerNameField();
            TribeId = _this->TargetingTeamField();
            _this->GetPlayerOrTribeName(&TribeNameStr);

            FString UniqueIdStr;
            _this->GetUniqueIdString(&UniqueIdStr);
            if (!UniqueIdStr.IsEmpty())
            {
                try {
                    SteamId = std::stoull(UniqueIdStr.ToString());
                } catch (...) {
                    SteamId = 0;
                }
            }
        }

        FString Timestamp = GetCurrentTimestamp();
        std::string playerNameStr = PlayerName.ToString();
        std::string tribeNameStdStr = TribeNameStr.ToString();
        std::string timestampStr = Timestamp.ToString();

        QueueTribeEvent(playerNameStr, SteamId, tribeNameStdStr, TribeId, "leave", timestampStr);

        if (LogToFile)
        {
            EnsureLogDirectoryExists();

            std::string logPathStr = LogFilePath.ToString();
            std::string filePath = logPathStr + "tribes.log";
            std::ofstream file(filePath, std::ios::app);
            if (file.is_open())
            {
                file << "[" << timestampStr << "] TRIBE LEFT: " << playerNameStr
                     << " (SteamID: " << SteamId << ") left tribe '"
                     << tribeNameStdStr << "' (ID: " << TribeId << ")\n";
                file.close();
            }
        }

        AShooterPlayerState_ClearTribe_original(_this, bDontRemoveFromTribe, bForce, ForPC);
    }

    void Hook_AShooterPlayerState_ServerRequestRenameTribe(AShooterPlayerState* _this, FString* NewTribeName)
    {
        FString PlayerName = TEXT("Unknown");
        uint64 SteamId = 0;
        FString OldTribeName = TEXT("Unknown Tribe");
        FString NewName = NewTribeName ? *NewTribeName : TEXT("Unknown");
        int TribeId = 0;

        if (_this)
        {
            PlayerName = _this->PlayerNameField();
            TribeId = _this->TargetingTeamField();
            _this->GetPlayerOrTribeName(&OldTribeName);

            FString UniqueIdStr;
            _this->GetUniqueIdString(&UniqueIdStr);
            if (!UniqueIdStr.IsEmpty())
            {
                try {
                    SteamId = std::stoull(UniqueIdStr.ToString());
                } catch (...) {
                    SteamId = 0;
                }
            }
        }

        FString Timestamp = GetCurrentTimestamp();
        std::string playerNameStr = PlayerName.ToString();
        std::string oldTribeNameStr = OldTribeName.ToString();
        std::string newTribeNameStr = NewName.ToString();
        std::string timestampStr = Timestamp.ToString();

        if (SendToApi && !ApiUrl.empty())
        {
            std::string mapName = GetCurrentMapName();

            nlohmann::json payload;
            payload["event_type"] = "tribe_rename";
            payload["player_name"] = playerNameStr;
            payload["steam_id"] = std::to_string(SteamId);
            payload["old_tribe_name"] = oldTribeNameStr;
            payload["new_tribe_name"] = newTribeNameStr;
            payload["tribe_id"] = TribeId;
            payload["map"] = mapName;
            payload["timestamp"] = timestampStr;

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
                        Log::GetLog()->warn("KillTracker: Tribe rename API request failed: {}", response);
                    }
                },
                jsonData,
                "application/json",
                headers
            );
        }

        if (LogToFile)
        {
            EnsureLogDirectoryExists();

            std::string logPathStr = LogFilePath.ToString();
            std::string filePath = logPathStr + "tribes.log";
            std::ofstream file(filePath, std::ios::app);
            if (file.is_open())
            {
                file << "[" << timestampStr << "] TRIBE RENAMED: " << playerNameStr
                     << " (SteamID: " << SteamId << ") renamed tribe '"
                     << oldTribeNameStr << "' to '" << newTribeNameStr
                     << "' (ID: " << TribeId << ")\n";
                file.close();
            }
        }

        AShooterPlayerState_ServerRequestRenameTribe_original(_this, NewTribeName);
    }
}
