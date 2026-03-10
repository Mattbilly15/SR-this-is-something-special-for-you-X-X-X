#define _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING

#include "KillDeathTracker_Shared.h"
#include "json.hpp"
#include "Requests.h"
#include "Timer.h"
#include "API/ARK/Enums.h"
#include <algorithm>

namespace KillTracker
{
    // DECLARE_HOOK macros moved to KillDeathTracker_Shared.h

    void Hook_AShooterWeapon_ServerStartFire(AShooterWeapon* _this)
    {
        AShooterWeapon_ServerStartFire_original(_this);

        if (!_this)
            return;

        AShooterCharacter* Owner = _this->MyPawnField();
        if (!Owner)
            return;

        AShooterPlayerController* PlayerController = ArkApi::GetApiUtils().FindControllerFromCharacter(Owner);
        if (!PlayerController)
            return;

        uint64 SteamId = ArkApi::IApiUtils::GetSteamIdFromController(PlayerController);
        if (SteamId == 0)
            return;

        std::string weaponName = "Unknown Weapon";
        UPrimalItem* WeaponItem = _this->AssociatedPrimalItemField();
        if (WeaponItem)
        {
            FString WeaponNameF;
            WeaponItem->GetItemName(&WeaponNameF, false, false, nullptr);
            weaponName = WeaponNameF.ToString();
        }

        bool isProjectile = IsProjectileWeapon(weaponName);
        bool isHitscan = IsHitscanWeapon(weaponName);
        bool isBola = IsBolaWeapon(weaponName);

        if (!isProjectile && !isHitscan)
        {
            return;
        }

        FString PlayerNameF = ArkApi::IApiUtils::GetCharacterName(PlayerController);
        std::string playerNameStr = PlayerNameF.ToString();

        if (isBola)
        {
            PlayerBolaStats[SteamId].playerName = playerNameStr;
        }
        else
        {
            PlayerShotStats[SteamId].lastWeaponUsed = weaponName;
            PlayerShotStats[SteamId].playerName = playerNameStr;
        }

        FVector PlayerLocation(0, 0, 0);
        if (Owner)
        {
            USceneComponent* RootComp = Owner->RootComponentField();
            if (RootComp)
            {
                RootComp->GetWorldLocation(&PlayerLocation);
            }
        }

        auto now = std::chrono::system_clock::now();

        if (isProjectile)
        {
            RecentShot shot;
            shot.steamId = SteamId;
            shot.weaponName = weaponName;
            shot.playerName = playerNameStr;
            shot.location = PlayerLocation;
            shot.shotTime = now;
            shot.hasHit = false;
            shot.actuallyFired = false;
            shot.hitTarget = "";
            shot.hitTargetName = "";
            shot.boneName = "";
            shot.missCheckTime = std::chrono::system_clock::time_point();

            RecentShots.push_back(shot);
        }
        else if (isHitscan)
        {
            RecentShot shot;
            shot.steamId = SteamId;
            shot.weaponName = weaponName;
            shot.playerName = playerNameStr;
            shot.location = PlayerLocation;
            shot.shotTime = now;
            shot.hasHit = false;
            shot.actuallyFired = false;
            shot.hitTarget = "";
            shot.hitTargetName = "";
            shot.boneName = "";
            shot.missCheckTime = now + std::chrono::seconds(5);

            RecentShots.push_back(shot);
        }

        RecentShots.erase(
            std::remove_if(RecentShots.begin(), RecentShots.end(),
                [now](const RecentShot& s) {
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - s.shotTime).count();
                    return elapsed > 5000;
                }),
            RecentShots.end());
    }

    void Hook_AShooterWeapon_DealDamage(AShooterWeapon* _this,
                                        FHitResult* Impact,
                                        FVector* ShootDir,
                                        float DamageAmount,
                                        TSubclassOf<UDamageType> DamageType,
                                        float Impulse,
                                        AActor* DamageCauserOverride)
    {
        AShooterWeapon_DealDamage_original(_this, Impact, ShootDir, DamageAmount, DamageType, Impulse, DamageCauserOverride);

        AShooterCharacter* Owner = nullptr;
        AShooterPlayerController* AttackerController = nullptr;
        uint64 AttackerSteamId = 0;
        std::string weaponName = "Unknown Weapon";
        std::string playerNameStr = "Unknown";
        FVector PlayerLocation(0, 0, 0);
        bool isHitscan = false;

        if (_this)
        {
            try
            {
                Owner = _this->MyPawnField();
                if (Owner)
                {
                    AttackerController = ArkApi::GetApiUtils().FindControllerFromCharacter(Owner);
                    if (AttackerController)
                    {
                        AttackerSteamId = ArkApi::IApiUtils::GetSteamIdFromController(AttackerController);

                        UPrimalItem* WeaponItem = _this->AssociatedPrimalItemField();
                        if (WeaponItem)
                        {
                            FString WeaponNameF;
                            WeaponItem->GetItemName(&WeaponNameF, false, false, nullptr);
                            weaponName = WeaponNameF.ToString();
                            isHitscan = IsHitscanWeapon(weaponName);
                        }

                        FString PlayerNameF = ArkApi::IApiUtils::GetCharacterName(AttackerController);
                        playerNameStr = PlayerNameF.ToString();

                        USceneComponent* RootComp = Owner->RootComponentField();
                        if (RootComp)
                        {
                            RootComp->GetWorldLocation(&PlayerLocation);
                        }
                    }
                }
            }
            catch (...) {}
        }

        if (_this && AttackerSteamId != 0)
        {
            try
            {
                AActor* HitActor = nullptr;
                if (Impact)
                {
                    HitActor = Impact->GetActor();
                }
                
                if (HitActor && HitActor->IsA(AShooterCharacter::GetPrivateStaticClass()))
                {
                    AShooterCharacter* VictimChar = static_cast<AShooterCharacter*>(HitActor);
                    AShooterPlayerController* VictimController = ArkApi::GetApiUtils().FindControllerFromCharacter(VictimChar);
                    if (VictimController)
                    {
                        uint64 VictimSteamId = ArkApi::IApiUtils::GetSteamIdFromController(VictimController);
                        if (VictimSteamId != 0 && VictimSteamId != AttackerSteamId)
                        {
                            if (IsBolaWeapon(weaponName))
                            {
                                PlayerBolaStats[AttackerSteamId].playerHits++;
                            }
                            else
                            {
                                PlayerShotStats[AttackerSteamId].playerHits++;
                            }

                            try
                            {
                                if (Impact)
                                {
                                    FName BoneName = Impact->BoneNameField();
                                    FString BoneNameStr = BoneName.ToString();
                                    std::string boneNameStr = BoneNameStr.ToString();

                                    if (!boneNameStr.empty() && boneNameStr != "None" && BoneName.ComparisonIndex != 0)
                                    {
                                        std::transform(boneNameStr.begin(), boneNameStr.end(), boneNameStr.begin(), ::tolower);
                                        std::string bodyPart = GetBodyPartFromBone(boneNameStr);

                                        if (PlayerBodyPartStats[AttackerSteamId].playerName.empty())
                                        {
                                            FString AttackerName = ArkApi::IApiUtils::GetCharacterName(AttackerController);
                                            PlayerBodyPartStats[AttackerSteamId].playerName = AttackerName.ToString();
                                        }

                                        if (bodyPart == "head")
                                        {
                                            PlayerBodyPartStats[AttackerSteamId].headHits++;
                                        }
                                        else if (bodyPart == "neck")
                                        {
                                            PlayerBodyPartStats[AttackerSteamId].neckHits++;
                                        }
                                        else if (bodyPart == "chest")
                                        {
                                            PlayerBodyPartStats[AttackerSteamId].chestHits++;
                                        }
                                        else if (bodyPart == "stomach")
                                        {
                                            PlayerBodyPartStats[AttackerSteamId].stomachHits++;
                                        }
                                        else if (bodyPart == "arm")
                                        {
                                            PlayerBodyPartStats[AttackerSteamId].armHits++;
                                        }
                                        else if (bodyPart == "hand")
                                        {
                                            PlayerBodyPartStats[AttackerSteamId].handHits++;
                                        }
                                        else if (bodyPart == "leg")
                                        {
                                            PlayerBodyPartStats[AttackerSteamId].legHits++;
                                        }
                                        else if (bodyPart == "foot")
                                        {
                                            PlayerBodyPartStats[AttackerSteamId].footHits++;
                                        }
                                        else
                                        {
                                            PlayerBodyPartStats[AttackerSteamId].otherHits++;
                                        }
                                    }
                                }
                            }
                            catch (...) {}
                        }
                    }
                }
                else if (HitActor && HitActor->IsA(APrimalDinoCharacter::GetPrivateStaticClass()))
                {
                    if (IsBolaWeapon(weaponName))
                    {
                        PlayerBolaStats[AttackerSteamId].dinoHits++;
                    }
                    else
                    {
                        PlayerShotStats[AttackerSteamId].dinoHits++;
                    }
                }
            }
            catch (...) {}
        }

        bool isProjectile = IsProjectileWeapon(weaponName);
        if (isProjectile && AttackerSteamId != 0)
        {
            try
            {
                bool hitTerrain = false;
                if (Impact)
                {
                    AActor* HitActor = Impact->GetActor();
                    UPrimitiveComponent* HitComponent = Impact->GetComponent();

                    if (!HitActor && HitComponent)
                    {
                        hitTerrain = true;
                    }
                }

                if (hitTerrain)
                {
                    auto now = std::chrono::system_clock::now();
                    RecentShot* matchedShot = nullptr;

                    for (auto it = RecentShots.rbegin(); it != RecentShots.rend(); ++it)
                    {
                        if (it->steamId == AttackerSteamId && it->actuallyFired && !it->hasHit && IsProjectileWeapon(it->weaponName))
                        {
                            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->shotTime).count();
                            if (elapsed < 5000)
                            {
                                matchedShot = &(*it);
                                break;
                            }
                        }
                    }

                    if (matchedShot)
                    {
                        matchedShot->hasHit = true;
                        matchedShot->hitTarget = "miss";
                    }
                }
            }
            catch (const std::exception& e)
            {
                Log::GetLog()->error("KillTracker: Exception in projectile terrain hit detection: {}", e.what());
            }
            catch (...)
            {
                Log::GetLog()->error("KillTracker: Unknown exception in projectile terrain hit detection");
            }
        }

        if (isHitscan && AttackerSteamId != 0)
        {
            try
            {
                auto now = std::chrono::system_clock::now();
                bool hitPlayerOrDino = false;
                if (Impact)
                {
                    AActor* HitActor = Impact->GetActor();
                    if (HitActor && (HitActor->IsA(AShooterCharacter::GetPrivateStaticClass()) || HitActor->IsA(APrimalDinoCharacter::GetPrivateStaticClass())))
                    {
                        hitPlayerOrDino = true;
                    }
                }

                RecentShot* existingShot = nullptr;
                for (auto it = RecentShots.rbegin(); it != RecentShots.rend(); ++it)
                {
                    if (it->steamId == AttackerSteamId &&
                        IsHitscanWeapon(it->weaponName))
                    {
                        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->shotTime).count();
                        if (elapsed < 2000)
                        {
                            if (it->weaponName == weaponName && !it->actuallyFired)
                            {
                                if (!existingShot)
                                {
                                    existingShot = &(*it);
                                }
                            }
                            else if (!it->actuallyFired)
                            {
                                it->hasHit = true;
                            }
                        }
                    }
                }

                if (existingShot)
                {
                    existingShot->actuallyFired = true;
                    existingShot->location = PlayerLocation;
                    existingShot->playerName = playerNameStr;

                    if (hitPlayerOrDino)
                    {
                        existingShot->hasHit = false;
                        existingShot->hitTarget = "";
                        existingShot->missCheckTime = std::chrono::system_clock::time_point();
                    }
                    else
                    {
                        existingShot->hasHit = false;
                        existingShot->hitTarget = "";
                        existingShot->missCheckTime = now + std::chrono::seconds(5);
                    }
                }
                else
                {
                    RecentShot shot;
                    shot.steamId = AttackerSteamId;
                    shot.weaponName = weaponName;
                    shot.playerName = playerNameStr;
                    shot.location = PlayerLocation;
                    shot.shotTime = now;
                    shot.actuallyFired = true;

                    if (hitPlayerOrDino)
                    {
                        shot.hasHit = false;
                        shot.hitTarget = "";
                        shot.missCheckTime = std::chrono::system_clock::time_point();
                    }
                    else
                    {
                        shot.hasHit = false;
                        shot.hitTarget = "";
                        shot.missCheckTime = now + std::chrono::seconds(5);
                    }
                    shot.hitTargetName = "";
                    shot.boneName = "";

                    RecentShots.push_back(shot);
                }

                bool isBola = IsBolaWeapon(weaponName);
                if (isBola)
                {
                    PlayerBolaStats[AttackerSteamId].totalChucks++;
                    PlayerBolaStats[AttackerSteamId].playerName = playerNameStr;
                }
                else
                {
                    PlayerShotStats[AttackerSteamId].totalShots++;
                    PlayerShotStats[AttackerSteamId].lastWeaponUsed = weaponName;
                    PlayerShotStats[AttackerSteamId].playerName = playerNameStr;
                }
            }
            catch (const std::exception& e)
            {
                Log::GetLog()->error("KillTracker: Exception in hitscan shot tracking: {}", e.what());
            }
            catch (...)
            {
                Log::GetLog()->error("KillTracker: Unknown exception in hitscan shot tracking");
            }
        }
    }

    float Hook_APrimalCharacter_TakeDamage(APrimalCharacter* _this, float Damage, FDamageEvent* DamageEvent,
                                            AController* EventInstigator, AActor* DamageCauser)
    {
        if (!_this)
        {
            return APrimalCharacter_TakeDamage_original(_this, Damage, DamageEvent, EventInstigator, DamageCauser);
        }

        float actualDamageTaken = APrimalCharacter_TakeDamage_original(_this, Damage, DamageEvent, EventInstigator, DamageCauser);

        try
        {
            if (!_this->IsA(AShooterCharacter::GetPrivateStaticClass()))
            {
                return actualDamageTaken;
            }

            AShooterCharacter* PlayerChar = static_cast<AShooterCharacter*>(_this);
            AShooterPlayerController* VictimController = nullptr;
            try
            {
                VictimController = ArkApi::GetApiUtils().FindControllerFromCharacter(PlayerChar);
            }
            catch (...)
            {
                return actualDamageTaken;
            }

            if (!VictimController)
            {
                return actualDamageTaken;
            }

            AShooterPlayerController* AttackerController = nullptr;
            uint64 AttackerSteamId = 0;

            if (EventInstigator && EventInstigator->IsA(AShooterPlayerController::GetPrivateStaticClass()))
            {
                AttackerController = static_cast<AShooterPlayerController*>(EventInstigator);
                AttackerSteamId = ArkApi::IApiUtils::GetSteamIdFromController(AttackerController);
            }
            else if (DamageCauser)
            {
                try
                {
                    APawn* Instigator = DamageCauser->InstigatorField();
                    if (Instigator && Instigator->IsA(AShooterCharacter::GetPrivateStaticClass()))
                    {
                        AShooterCharacter* ShooterChar = static_cast<AShooterCharacter*>(Instigator);
                        AttackerController = ArkApi::GetApiUtils().FindControllerFromCharacter(ShooterChar);
                        if (AttackerController)
                        {
                            AttackerSteamId = ArkApi::IApiUtils::GetSteamIdFromController(AttackerController);
                        }
                    }
                    if (!AttackerController)
                    {
                        AActor* Owner = DamageCauser->OwnerField();
                        if (Owner && Owner->IsA(AShooterCharacter::GetPrivateStaticClass()))
                        {
                            AShooterCharacter* OwnerChar = static_cast<AShooterCharacter*>(Owner);
                            AttackerController = ArkApi::GetApiUtils().FindControllerFromCharacter(OwnerChar);
                            if (AttackerController)
                            {
                                AttackerSteamId = ArkApi::IApiUtils::GetSteamIdFromController(AttackerController);
                            }
                        }
                    }
                }
                catch (...) {}
            }

            if (!AttackerController && AttackerSteamId == 0)
            {
                try
                {
                    AttackerSteamId = ArkApi::GetApiUtils().GetAttackerSteamID(_this, EventInstigator, DamageCauser, false);
                    if (AttackerSteamId != 0)
                    {
                        if (EventInstigator && EventInstigator->IsA(AShooterPlayerController::GetPrivateStaticClass()))
                        {
                            AShooterPlayerController* TestController = static_cast<AShooterPlayerController*>(EventInstigator);
                            uint64 TestSteamId = ArkApi::IApiUtils::GetSteamIdFromController(TestController);
                            if (TestSteamId == AttackerSteamId)
                            {
                                AttackerController = TestController;
                            }
                        }
                    }
                }
                catch (...) {}
            }

            if (AttackerController && AttackerSteamId != 0)
            {
                uint64 VictimSteamId = ArkApi::IApiUtils::GetSteamIdFromController(VictimController);

                if (AttackerSteamId != 0 && AttackerSteamId != VictimSteamId)
                {
                    std::string weaponName = "Unknown Weapon";
                    if (DamageCauser)
                    {
                        try
                        {
                            APawn* Instigator = DamageCauser->InstigatorField();
                            if (Instigator && Instigator->IsA(AShooterCharacter::GetPrivateStaticClass()))
                            {
                                AShooterCharacter* ShooterChar = static_cast<AShooterCharacter*>(Instigator);
                                AShooterWeapon* CurrentWeapon = ShooterChar->CurrentWeaponField();
                                if (CurrentWeapon)
                                {
                                    UPrimalItem* WeaponItem = CurrentWeapon->AssociatedPrimalItemField();
                                    if (WeaponItem)
                                    {
                                        FString WeaponNameF;
                                        WeaponItem->GetItemName(&WeaponNameF, false, false, nullptr);
                                        weaponName = WeaponNameF.ToString();
                                    }
                                }
                            }
                        }
                        catch (...) {}
                    }

                    std::string lowerWeaponName = weaponName;
                    std::transform(lowerWeaponName.begin(), lowerWeaponName.end(), lowerWeaponName.begin(), ::tolower);
                    bool isBola = (lowerWeaponName.find("bola") != std::string::npos || lowerWeaponName.find("net") != std::string::npos);
                    bool isCompoundBow = (lowerWeaponName.find("compound bow") != std::string::npos);

                    if (isBola)
                    {
                        PlayerBolaStats[AttackerSteamId].playerHits++;
                        if (PlayerBolaStats[AttackerSteamId].playerName.empty())
                        {
                            FString AttackerName = ArkApi::IApiUtils::GetCharacterName(AttackerController);
                            PlayerBolaStats[AttackerSteamId].playerName = AttackerName.ToString();
                        }
                    }
                    else if (isCompoundBow)
                    {
                        PlayerShotStats[AttackerSteamId].playerHits++;
                        if (PlayerShotStats[AttackerSteamId].playerName.empty())
                        {
                            FString AttackerName = ArkApi::IApiUtils::GetCharacterName(AttackerController);
                            PlayerShotStats[AttackerSteamId].playerName = AttackerName.ToString();
                        }
                    }

                    if ((isBola || isCompoundBow) && DamageEvent && _this && DamageCauser)
                    {
                        try
                        {
                            FHitResult HitInfo;
                            FVector ImpulseDir;
                            bool gotHitInfo = false;
                            
                            try
                            {
                                AActor* HitInstigator = DamageCauser;
                                if (!HitInstigator && EventInstigator && EventInstigator->IsA(AShooterPlayerController::GetPrivateStaticClass()))
                                {
                                    AShooterPlayerController* ShooterPC = static_cast<AShooterPlayerController*>(EventInstigator);
                                    ACharacter* Char = ShooterPC->CharacterField();
                                    if (Char && Char->IsA(AShooterCharacter::GetPrivateStaticClass()))
                                    {
                                        HitInstigator = static_cast<AShooterCharacter*>(Char);
                                    }
                                }
                                DamageEvent->GetBestHitInfo(_this, HitInstigator, &HitInfo, &ImpulseDir);
                                gotHitInfo = true;
                            }
                            catch (...) {}

                            if (gotHitInfo)
                            {
                                try
                                {
                                    FName& BoneNameRef = HitInfo.BoneNameField();
                                    FName BoneName = BoneNameRef;
                                    
                                    if (BoneName.ComparisonIndex != 0)
                                    {
                                        FString BoneNameStr = BoneName.ToString();
                                        std::string boneNameStr = BoneNameStr.ToString();

                                        if (!boneNameStr.empty() && boneNameStr != "None")
                                        {
                                            std::transform(boneNameStr.begin(), boneNameStr.end(), boneNameStr.begin(), ::tolower);
                                            std::string bodyPart = GetBodyPartFromBone(boneNameStr);

                                            if (PlayerBodyPartStats[AttackerSteamId].playerName.empty())
                                            {
                                                try
                                                {
                                                    FString AttackerName = ArkApi::IApiUtils::GetCharacterName(AttackerController);
                                                    PlayerBodyPartStats[AttackerSteamId].playerName = AttackerName.ToString();
                                                }
                                                catch (...) {}
                                            }

                                            if (bodyPart == "head")
                                            {
                                                PlayerBodyPartStats[AttackerSteamId].headHits++;
                                            }
                                            else if (bodyPart == "neck")
                                            {
                                                PlayerBodyPartStats[AttackerSteamId].neckHits++;
                                            }
                                            else if (bodyPart == "chest")
                                            {
                                                PlayerBodyPartStats[AttackerSteamId].chestHits++;
                                            }
                                            else if (bodyPart == "stomach")
                                            {
                                                PlayerBodyPartStats[AttackerSteamId].stomachHits++;
                                            }
                                            else if (bodyPart == "arm")
                                            {
                                                PlayerBodyPartStats[AttackerSteamId].armHits++;
                                            }
                                            else if (bodyPart == "hand")
                                            {
                                                PlayerBodyPartStats[AttackerSteamId].handHits++;
                                            }
                                            else if (bodyPart == "leg")
                                            {
                                                PlayerBodyPartStats[AttackerSteamId].legHits++;
                                            }
                                            else if (bodyPart == "foot")
                                            {
                                                PlayerBodyPartStats[AttackerSteamId].footHits++;
                                            }
                                            else
                                            {
                                                PlayerBodyPartStats[AttackerSteamId].otherHits++;
                                            }
                                        }
                                    }
                                }
                                catch (...) {}
                            }
                        }
                        catch (...) {}
                    }
                }
            }
        }
        catch (...) {}

        return actualDamageTaken;
    }

    void CheckPendingShots()
    {
        auto now = std::chrono::system_clock::now();

        for (auto& shot : RecentShots)
        {
            bool isHitscan = IsHitscanWeapon(shot.weaponName);
            bool isProjectile = IsProjectileWeapon(shot.weaponName);

            if (shot.actuallyFired && !shot.hasHit)
            {
                if (shot.missCheckTime > shot.shotTime)
                {
                    if (now >= shot.missCheckTime)
                    {
                        if (!shot.hasHit)
                        {
                            shot.hasHit = true;
                            shot.hitTarget = "miss";

                            bool isBola = IsBolaWeapon(shot.weaponName);
                            if (isBola)
                            {
                                PlayerBolaStats[shot.steamId].misses++;
                            }
                            else
                            {
                                PlayerShotStats[shot.steamId].totalShots++;
                            }
                        }
                    }
                }
                else
                {
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - shot.shotTime).count();
                    if (elapsed > 5000)
                    {
                        if (!shot.hasHit)
                        {
                            shot.hasHit = true;
                            shot.hitTarget = "miss";

                            bool isBola = IsBolaWeapon(shot.weaponName);
                            if (isBola)
                            {
                                PlayerBolaStats[shot.steamId].misses++;
                            }
                            else
                            {
                                PlayerShotStats[shot.steamId].totalShots++;
                            }
                        }
                    }
                }
            }
            else if (!shot.actuallyFired && isHitscan)
            {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - shot.shotTime).count();
                if (elapsed > 5000)
                {
                    shot.hasHit = true;
                    shot.hitTarget = "miss";
                    shot.actuallyFired = true;

                    bool isBola = IsBolaWeapon(shot.weaponName);
                    if (isBola)
                    {
                        PlayerBolaStats[shot.steamId].misses++;
                    }
                    else
                    {
                        PlayerShotStats[shot.steamId].totalShots++;
                    }
                }
            }
            else if (!shot.actuallyFired && isProjectile)
            {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - shot.shotTime).count();
                if (elapsed > 5000)
                {
                    shot.hasHit = true;
                }
            }
        }

        RecentShots.erase(
            std::remove_if(RecentShots.begin(), RecentShots.end(),
                [now](const RecentShot& s) {
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - s.shotTime).count();
                    return elapsed > 5000;
                }),
            RecentShots.end());
    }

    void ProcessProjectileBeginPlay(AActor* _this)
    {
        if (!_this)
            return;

        FString Blueprint = ArkApi::IApiUtils::GetBlueprint(_this);
        if (Blueprint.IsEmpty())
            return;

        std::string blueprintStr = Blueprint.ToString();
        std::transform(blueprintStr.begin(), blueprintStr.end(), blueprintStr.begin(), ::tolower);

        bool isProjectile = (blueprintStr.find("projectile") != std::string::npos ||
                            blueprintStr.find("bola") != std::string::npos ||
                            blueprintStr.find("arrow") != std::string::npos ||
                            blueprintStr.find("primalprojectile") != std::string::npos);

        if (isProjectile)
        {
            try
            {
                APawn* Instigator = _this->InstigatorField();
                if (Instigator && Instigator->IsA(AShooterCharacter::GetPrivateStaticClass()))
                {
                    AShooterCharacter* ShooterChar = static_cast<AShooterCharacter*>(Instigator);
                    AShooterPlayerController* PlayerController = ArkApi::GetApiUtils().FindControllerFromCharacter(ShooterChar);
                    if (PlayerController)
                    {
                        uint64 SteamId = ArkApi::IApiUtils::GetSteamIdFromController(PlayerController);
                        if (SteamId != 0)
                        {
                            auto now = std::chrono::system_clock::now();
                            for (auto it = RecentShots.rbegin(); it != RecentShots.rend(); ++it)
                            {
                                if (it->steamId == SteamId && !it->actuallyFired)
                                {
                                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->shotTime).count();
                                    if (elapsed < 2000)
                                    {
                                        it->actuallyFired = true;

                                        bool isBola = IsBolaWeapon(it->weaponName);
                                        if (isBola)
                                        {
                                            PlayerBolaStats[SteamId].totalChucks++;
                                            PlayerBolaStats[SteamId].playerName = it->playerName;
                                        }
                                        else
                                        {
                                            PlayerShotStats[SteamId].totalShots++;
                                            PlayerShotStats[SteamId].lastWeaponUsed = it->weaponName;
                                            PlayerShotStats[SteamId].playerName = it->playerName;
                                        }

                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            catch (...) {}
        }
    }
}
