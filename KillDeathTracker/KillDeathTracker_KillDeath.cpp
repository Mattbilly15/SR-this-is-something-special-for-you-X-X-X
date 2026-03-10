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

    bool Hook_AShooterCharacter_Die(AShooterCharacter* _this, float KillingDamage, FDamageEvent* DamageEvent,
                                     AController* Killer, AActor* DamageCauser)
    {
        if (!_this)
            return AShooterCharacter_Die_original(_this, KillingDamage, DamageEvent, Killer, DamageCauser);

        AShooterPlayerController* VictimController = nullptr;
        FString VictimName = TEXT("Unknown");
        uint64 VictimSteamId = 0;

        VictimController = ArkApi::GetApiUtils().FindControllerFromCharacter(_this);
        if (!VictimController)
            return AShooterCharacter_Die_original(_this, KillingDamage, DamageEvent, Killer, DamageCauser);

        VictimName = ArkApi::IApiUtils::GetCharacterName(VictimController);
        VictimSteamId = ArkApi::IApiUtils::GetSteamIdFromController(VictimController);

        FString KillerName = TEXT("Unknown");
        uint64 KillerSteamId = 0;
        FString WeaponName = TEXT("Unknown");
        bool IsPvPKill = false;

        if (Killer)
        {
            if (Killer->IsA(AShooterPlayerController::GetPrivateStaticClass()))
            {
                AShooterPlayerController* KillerPlayerController = static_cast<AShooterPlayerController*>(Killer);

                IsPvPKill = true;

                KillerName = ArkApi::IApiUtils::GetCharacterName(KillerPlayerController);
                KillerSteamId = ArkApi::IApiUtils::GetSteamIdFromController(KillerPlayerController);

                AShooterCharacter* KillerCharacter = KillerPlayerController->GetPlayerCharacter();
                if (KillerCharacter)
                {
                    AShooterWeapon* CurrentWeapon = KillerCharacter->GetCurrentWeapon();
                    if (CurrentWeapon)
                    {
                        UPrimalItem* WeaponItem = CurrentWeapon->AssociatedPrimalItemField();
                        if (WeaponItem)
                        {
                            FString TempWeaponName;
                            WeaponItem->GetItemName(&TempWeaponName, false, false, nullptr);
                            if (!TempWeaponName.IsEmpty())
                            {
                                WeaponName = TempWeaponName;
                            }
                        }
                    }
                }

                if (WeaponName.Equals(TEXT("Unknown")) && DamageCauser)
                {
                    FString Blueprint = ArkApi::IApiUtils::GetBlueprint(DamageCauser);
                    if (!Blueprint.IsEmpty())
                    {
                        WeaponName = Blueprint;
                    }
                }
            }
        }

        if (!IsPvPKill && DamageCauser && DamageCauser->IsA(APrimalDinoCharacter::GetPrivateStaticClass()))
        {
            APrimalDinoCharacter* DinoKiller = static_cast<APrimalDinoCharacter*>(DamageCauser);

            TWeakObjectPtr<AShooterCharacter> RiderRef = DinoKiller->RiderField();
            AShooterCharacter* Rider = RiderRef.Get();

            if (Rider)
            {
                AShooterPlayerController* RiderController = ArkApi::GetApiUtils().FindControllerFromCharacter(Rider);
                if (RiderController)
                {
                    IsPvPKill = true;

                    KillerName = ArkApi::IApiUtils::GetCharacterName(RiderController);
                    KillerSteamId = ArkApi::IApiUtils::GetSteamIdFromController(RiderController);

                    FString DinoName;
                    DinoKiller->GetDinoDescriptiveName(&DinoName);
                    if (!DinoName.IsEmpty())
                    {
                        WeaponName = DinoName + TEXT(" (Mounted)");
                    }
                    else
                    {
                        WeaponName = TEXT("Dinosaur (Mounted)");
                    }
                }
            }
        }

        FString DeathCause = TEXT("Unknown");
        if (!IsPvPKill)
        {
            if (DamageCauser)
            {
                if (DamageCauser->IsA(APrimalDinoCharacter::GetPrivateStaticClass()))
                {
                    APrimalDinoCharacter* DinoKiller = static_cast<APrimalDinoCharacter*>(DamageCauser);
                    FString DinoName;
                    DinoKiller->GetDinoDescriptiveName(&DinoName);
                    if (!DinoName.IsEmpty())
                    {
                        DeathCause = DinoName;
                    }
                    else
                    {
                        DeathCause = TEXT("Creature");
                    }
                }
                else
                {
                    FString Blueprint = ArkApi::IApiUtils::GetBlueprint(DamageCauser);
                    if (!Blueprint.IsEmpty())
                    {
                        DeathCause = Blueprint;
                    }
                }
            }
            else if (Killer)
            {
                APawn* KillerPawn = Killer->PawnField();
                if (KillerPawn && KillerPawn->IsA(APrimalDinoCharacter::GetPrivateStaticClass()))
                {
                    APrimalDinoCharacter* DinoCharacter = static_cast<APrimalDinoCharacter*>(KillerPawn);
                    FString DinoName;
                    DinoCharacter->GetDinoDescriptiveName(&DinoName);
                    if (!DinoName.IsEmpty())
                    {
                        DeathCause = DinoName;
                    }
                }
            }
            else
            {
                DeathCause = TEXT("Environmental Hazard");
            }
        }

        auto bossFightIt = PlayersInBossFight.find(VictimSteamId);
        if (bossFightIt != PlayersInBossFight.end())
        {
            auto now = std::chrono::system_clock::now();
            auto timeSinceStart = std::chrono::duration_cast<std::chrono::milliseconds>(now - bossFightIt->second.startTime).count();

            if (timeSinceStart < BOSS_FIGHT_TIMEOUT_MS && !bossFightIt->second.completed)
            {
                int durationSeconds = static_cast<int>(timeSinceStart / 1000);

                std::string killedBy = DeathCause.ToString();
                if (IsPvPKill)
                    killedBy = "PvP during boss fight";

                QueueBossEvent("boss_failed", killedBy, bossFightIt->second.difficulty,
                                   VictimName.ToString(), VictimSteamId, 1, durationSeconds);

                if (LogToFile)
                {
                    EnsureLogDirectoryExists();
                    std::string logPathStr = LogFilePath.ToString();
                    std::string filePath = logPathStr + "boss_fights.log";
                    std::ofstream file(filePath, std::ios::app);
                    if (file.is_open())
                    {
                        FString Timestamp = GetCurrentTimestamp();
                        file << "[" << Timestamp.ToString() << "] BOSS FIGHT FAILED: "
                             << VictimName.ToString() << " died after " << durationSeconds
                             << " seconds. Killed by: " << killedBy << "\n";
                        file.close();
                    }
                }

                PlayersInBossFight.erase(bossFightIt);
            }
        }

        if (IsPvPKill)
        {
            if (VictimSteamId != KillerSteamId)
            {
                LogKillEvent(KillerName, KillerSteamId, VictimName, VictimSteamId, WeaponName);
                BroadcastKillMessage(KillerName, VictimName);
            }
            else
            {
                LogDeathEvent(VictimName, VictimSteamId, TEXT("Suicide"));
            }
        }
        else
        {
            LogDeathEvent(VictimName, VictimSteamId, DeathCause);
        }

        return AShooterCharacter_Die_original(_this, KillingDamage, DamageEvent, Killer, DamageCauser);
    }

    bool Hook_APrimalDinoCharacter_Die(APrimalDinoCharacter* _this, float KillingDamage, FDamageEvent* DamageEvent,
                                        AController* Killer, AActor* DamageCauser)
    {
        if (!_this)
            return APrimalDinoCharacter_Die_original(_this, KillingDamage, DamageEvent, Killer, DamageCauser);

        if (_this->IsBossDino())
        {
            FString BossName;
            _this->GetDinoDescriptiveName(&BossName);
            std::string bossNameStr = BossName.ToString();

            std::string difficulty = "Unknown";
            std::string lowerName = bossNameStr;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

            if (lowerName.find("gamma") != std::string::npos)
                difficulty = "Gamma";
            else if (lowerName.find("beta") != std::string::npos)
                difficulty = "Beta";
            else if (lowerName.find("alpha") != std::string::npos)
                difficulty = "Alpha";

            int participantCount = 0;
            auto now = std::chrono::system_clock::now();

            for (auto& pair : PlayersInBossFight)
            {
                auto timeSinceStart = std::chrono::duration_cast<std::chrono::milliseconds>(now - pair.second.startTime).count();
                if (timeSinceStart < BOSS_FIGHT_TIMEOUT_MS && !pair.second.completed)
                {
                    pair.second.completed = true;
                    int durationSeconds = static_cast<int>(timeSinceStart / 1000);

                    QueueBossEvent("boss_completed", bossNameStr, difficulty,
                                       pair.second.bossName, pair.first, 0, durationSeconds);
                    participantCount++;
                }
            }

            FString Timestamp = GetCurrentTimestamp();
            if (LogToFile)
            {
                EnsureLogDirectoryExists();
                std::string logPathStr = LogFilePath.ToString();
                std::string filePath = logPathStr + "boss_fights.log";
                std::ofstream file(filePath, std::ios::app);
                if (file.is_open())
                {
                    file << "[" << Timestamp.ToString() << "] BOSS KILLED: " << bossNameStr
                         << " (" << difficulty << ") - " << participantCount << " participants\n";
                    file.close();
                }
            }

            for (auto it = PlayersInBossFight.begin(); it != PlayersInBossFight.end(); )
            {
                if (it->second.completed)
                    it = PlayersInBossFight.erase(it);
                else
                    ++it;
            }

            return APrimalDinoCharacter_Die_original(_this, KillingDamage, DamageEvent, Killer, DamageCauser);
        }

        int TamingTeamID = _this->TamingTeamIDField();
        if (TamingTeamID <= 0)
        {
            return APrimalDinoCharacter_Die_original(_this, KillingDamage, DamageEvent, Killer, DamageCauser);
        }

        FString DinoName;
        _this->GetDinoDescriptiveName(&DinoName);

        FString OwnerName = _this->TamerStringField();
        if (OwnerName.IsEmpty())
        {
            OwnerName = _this->TribeNameField();
        }
        if (OwnerName.IsEmpty())
        {
            OwnerName = TEXT("Unknown Owner");
        }

        FString KillerName = TEXT("Unknown");
        uint64 KillerSteamId = 0;
        bool WasKilledByPlayer = false;
        AShooterPlayerController* KillerController = nullptr;

        if (Killer && Killer->IsA(AShooterPlayerController::GetPrivateStaticClass()))
        {
            KillerController = static_cast<AShooterPlayerController*>(Killer);
            WasKilledByPlayer = true;
        }
        else if (DamageCauser)
        {
            APawn* Instigator = DamageCauser->InstigatorField();
            if (Instigator && Instigator->IsA(AShooterCharacter::GetPrivateStaticClass()))
            {
                AShooterCharacter* ShooterChar = static_cast<AShooterCharacter*>(Instigator);
                KillerController = ArkApi::GetApiUtils().FindControllerFromCharacter(ShooterChar);
                if (KillerController)
                    WasKilledByPlayer = true;
            }

            if (!KillerController)
            {
                AActor* Owner = DamageCauser->OwnerField();
                if (Owner && Owner->IsA(AShooterCharacter::GetPrivateStaticClass()))
                {
                    AShooterCharacter* OwnerChar = static_cast<AShooterCharacter*>(Owner);
                    KillerController = ArkApi::GetApiUtils().FindControllerFromCharacter(OwnerChar);
                    if (KillerController)
                        WasKilledByPlayer = true;
                }
            }

            if (!KillerController)
            {
                AController* CauserController = DamageCauser->GetInstigatorController();
                if (CauserController && CauserController->IsA(AShooterPlayerController::GetPrivateStaticClass()))
                {
                    KillerController = static_cast<AShooterPlayerController*>(CauserController);
                    WasKilledByPlayer = true;
                }
            }

            if (!KillerController && DamageCauser->IsA(APrimalDinoCharacter::GetPrivateStaticClass()))
            {
                APrimalDinoCharacter* DinoKiller = static_cast<APrimalDinoCharacter*>(DamageCauser);

                TWeakObjectPtr<AShooterCharacter> RiderRef = DinoKiller->RiderField();
                AShooterCharacter* Rider = RiderRef.Get();

                if (Rider)
                {
                    KillerController = ArkApi::GetApiUtils().FindControllerFromCharacter(Rider);
                    if (KillerController)
                        WasKilledByPlayer = true;
                }
            }
        }

        if (KillerController)
        {
            KillerName = ArkApi::IApiUtils::GetCharacterName(KillerController);
            KillerSteamId = ArkApi::IApiUtils::GetSteamIdFromController(KillerController);
        }

        if (WasKilledByPlayer && KillerSteamId != 0)
        {
            FString Timestamp = GetCurrentTimestamp();

            std::string killerNameStr = KillerName.ToString();
            std::string dinoNameStr = DinoName.ToString();
            std::string ownerNameStr = OwnerName.ToString();
            std::string timestampStr = Timestamp.ToString();

            QueueTamedDinoKillEvent(killerNameStr, KillerSteamId, dinoNameStr, timestampStr);

            if (LogToFile)
            {
                EnsureLogDirectoryExists();

                std::string logPathStr = LogFilePath.ToString();
                std::string filePath = logPathStr + "dino_kills.log";
                std::ofstream file(filePath, std::ios::app);
                if (file.is_open())
                {
                    file << "[" << timestampStr << "] TAMED DINO KILL: " << killerNameStr
                         << " (SteamID: " << KillerSteamId << ") killed "
                         << dinoNameStr << " owned by " << ownerNameStr << "\n";
                    file.close();
                }

                std::string combinedFilePath = logPathStr + "all_events.log";
                std::ofstream combinedFile(combinedFilePath, std::ios::app);
                if (combinedFile.is_open())
                {
                    combinedFile << "[" << timestampStr << "] TAMED DINO KILL: " << killerNameStr
                                 << " (SteamID: " << KillerSteamId << ") killed "
                                 << dinoNameStr << " owned by " << ownerNameStr << "\n";
                    combinedFile.close();
                }
            }
        }
        return APrimalDinoCharacter_Die_original(_this, KillingDamage, DamageEvent, Killer, DamageCauser);
    }
}
