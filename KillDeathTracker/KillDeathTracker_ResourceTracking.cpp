#define _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING

#include "KillDeathTracker_Shared.h"
#include "json.hpp"
#include "Requests.h"
#include "Timer.h"
#include "API/ARK/Enums.h"
#include <algorithm>

namespace KillTracker
{
    // FAnimNotifyEvent and UAnimInstance structs moved to KillDeathTracker_Shared.h
    // DECLARE_HOOK macros moved to KillDeathTracker_Shared.h

    void Hook_UPrimalHarvestingComponent_DealDirectDamage(UPrimalHarvestingComponent* _this, APlayerController* ForPC,
                                                          float DamageAmount, TSubclassOf<UDamageType> DamageTypeClass)
    {
        UPrimalHarvestingComponent_DealDirectDamage_original(_this, ForPC, DamageAmount, DamageTypeClass);

        if (!ForPC)
            return;

        AShooterPlayerController* ShooterPC = static_cast<AShooterPlayerController*>(ForPC);
        if (!ShooterPC)
            return;

        uint64 SteamId = ArkApi::IApiUtils::GetSteamIdFromController(ShooterPC);
        if (SteamId == 0)
            return;

        HarvestInfo info;
        info.harvestTime = std::chrono::system_clock::now();
        info.steamId = SteamId;

        FString PlayerName = ArkApi::IApiUtils::GetCharacterName(ShooterPC);
        info.playerName = PlayerName.ToString();

        AShooterCharacter* Character = static_cast<AShooterCharacter*>(ShooterPC->CharacterField());
        if (Character)
        {
            APrimalDinoCharacter* Mount = Character->GetRidingDino();
            if (Mount)
            {
                FString DinoName;
                Mount->GetDinoDescriptiveName(&DinoName);
                info.harvestMethod = DinoName.ToString();
            }
            else
            {
                info.harvestMethod = "On Foot";
            }
        }
        else
        {
            info.harvestMethod = "On Foot";
        }

        RecentHarvesters[SteamId] = info;
    }

    void Hook_UPrimalInventoryComponent_NotifyItemAdded(UPrimalInventoryComponent* _this, UPrimalItem* theItem, bool bEquippedItem)
    {
        UPrimalInventoryComponent_NotifyItemAdded_original(_this, theItem, bEquippedItem);

        if (!_this || !theItem)
            return;

        FString ItemName;
        theItem->GetItemName(&ItemName, false, false, nullptr);
        std::string itemNameStr = ItemName.ToString();

        std::string resourceType = GetTrackedResourceType(itemNameStr);
        if (resourceType.empty())
            return;

        int quantity = theItem->GetItemQuantity();

        AActor* Owner = _this->GetOwner();
        if (!Owner)
            return;

        uint64 OwnerSteamId = 0;
        std::string ownerName = "";

        if (Owner->IsA(AShooterCharacter::GetPrivateStaticClass()))
        {
            AShooterCharacter* PlayerChar = static_cast<AShooterCharacter*>(Owner);
            AShooterPlayerController* PC = ArkApi::GetApiUtils().FindControllerFromCharacter(PlayerChar);
            if (PC)
            {
                OwnerSteamId = ArkApi::IApiUtils::GetSteamIdFromController(PC);
                FString Name = ArkApi::IApiUtils::GetCharacterName(PC);
                ownerName = Name.ToString();
            }
        }
        else if (Owner->IsA(APrimalDinoCharacter::GetPrivateStaticClass()))
        {
            APrimalDinoCharacter* Dino = static_cast<APrimalDinoCharacter*>(Owner);
            TWeakObjectPtr<AShooterCharacter> RiderRef = Dino->RiderField();
            AShooterCharacter* Rider = RiderRef.Get();
            if (Rider)
            {
                AShooterPlayerController* PC = ArkApi::GetApiUtils().FindControllerFromCharacter(Rider);
                if (PC)
                {
                    OwnerSteamId = ArkApi::IApiUtils::GetSteamIdFromController(PC);
                    FString Name = ArkApi::IApiUtils::GetCharacterName(PC);
                    ownerName = Name.ToString();
                }
            }
        }

        if (OwnerSteamId == 0)
            return;

        auto harvesterIt = RecentHarvesters.find(OwnerSteamId);
        if (harvesterIt == RecentHarvesters.end())
            return;

        auto now = std::chrono::system_clock::now();
        auto timeSinceHarvest = std::chrono::duration_cast<std::chrono::milliseconds>(now - harvesterIt->second.harvestTime).count();

        if (timeSinceHarvest > HARVEST_CORRELATION_WINDOW_MS)
            return;

        PlayerResourceStats[OwnerSteamId].playerName = ownerName;

        if (resourceType == "metal")
            PlayerResourceStats[OwnerSteamId].metal += quantity;
        else if (resourceType == "flint")
            PlayerResourceStats[OwnerSteamId].flint += quantity;
        else if (resourceType == "stone")
            PlayerResourceStats[OwnerSteamId].stone += quantity;
        else if (resourceType == "pearl")
            PlayerResourceStats[OwnerSteamId].pearl += quantity;
        else if (resourceType == "black_pearl")
            PlayerResourceStats[OwnerSteamId].blackPearl += quantity;
        else if (resourceType == "oil")
            PlayerResourceStats[OwnerSteamId].oil += quantity;
        else if (resourceType == "sulfur")
            PlayerResourceStats[OwnerSteamId].sulfur += quantity;
        else if (resourceType == "fiber")
            PlayerResourceStats[OwnerSteamId].fiber += quantity;
        else if (resourceType == "crystal")
            PlayerResourceStats[OwnerSteamId].crystal += quantity;
        else if (resourceType == "wood")
            PlayerResourceStats[OwnerSteamId].wood += quantity;
        else if (resourceType == "thatch")
            PlayerResourceStats[OwnerSteamId].thatch += quantity;
        else if (resourceType == "element")
            PlayerResourceStats[OwnerSteamId].element += quantity;
        else if (resourceType == "element_shards")
            PlayerResourceStats[OwnerSteamId].elementShards += quantity;
        else if (resourceType == "charcoal")
            PlayerResourceStats[OwnerSteamId].charcoal += quantity;
        else if (resourceType == "organic_poly")
            PlayerResourceStats[OwnerSteamId].organicPoly += quantity;
        else if (resourceType == "hide")
            PlayerResourceStats[OwnerSteamId].hide += quantity;
        else if (resourceType == "keratin")
            PlayerResourceStats[OwnerSteamId].keratin += quantity;
        else if (resourceType == "chitin")
            PlayerResourceStats[OwnerSteamId].chitin += quantity;
        else if (resourceType == "megachelon_shells")
            PlayerResourceStats[OwnerSteamId].megachelonShells += quantity;
        else if (resourceType == "obsidian")
            PlayerResourceStats[OwnerSteamId].obsidian += quantity;
    }

    void Hook_UPrimalHarvestingComponent_GiveHarvestResource(UPrimalHarvestingComponent* _this, UPrimalInventoryComponent* invComp, float AdditionalEffectiveness, TSubclassOf<UDamageType> DamageTypeClass, AActor* ToActor, TArray<FHarvestResourceEntry>* HarvestResourceEntryOverrides)
    {
        if (ToActor && ToActor->TargetingTeamField() >= 50000)
            FromHarvest = true;

        UPrimalHarvestingComponent_GiveHarvestResource_original(_this, invComp, AdditionalEffectiveness, DamageTypeClass, ToActor, HarvestResourceEntryOverrides);

        FromHarvest = false;
    }

    void Hook_UAnimInstance_TriggerAnimNotifies(::UAnimInstance* _this, float DeltaSeconds, __int64 bForceFireNotifies)
    {
        static FName DelicateHarvest("HarvestEnd", EFindName::FNAME_Find);

        if (_this)
        {
            for (const auto& AnimNotify : _this->AnimNotifiesField())
            {
                if (!AnimNotify)
                {
                    continue;
                }

                if (AnimNotify->NotifyNameField() == DelicateHarvest)
                {
                    FromHarvest = true;
                }
            }
        }

        UAnimInstance_TriggerAnimNotifies_original(_this, DeltaSeconds, bForceFireNotifies);

        FromHarvest = false;
    }

    int Hook_UPrimalInventoryComponent_IncrementItemTemplateQuantity(UPrimalInventoryComponent* _this, TSubclassOf<UPrimalItem> ItemTemplate, int amount, bool bReplicateToClient, bool bIsBlueprint, UPrimalItem** UseSpecificItem, UPrimalItem** IncrementedItem, bool bRequireExactClassMatch, bool bIsCraftingResourceConsumption, bool bIsFromUseConsumption, bool bIsArkTributeItem, bool ShowHUDNotification, bool bDontRecalcSpoilingTime, bool bDontExceedMaxItems)
    {
        bool isFromHarvest = FromHarvest;

        if (!isFromHarvest && _this && amount > 0)
        {
            AActor* InvOwner = _this->GetOwner();
            uint64 OwnerSteamId = 0;

            if (InvOwner->IsA(AShooterCharacter::GetPrivateStaticClass()))
            {
                AShooterCharacter* PlayerChar = static_cast<AShooterCharacter*>(InvOwner);
                AShooterPlayerController* PC = ArkApi::GetApiUtils().FindControllerFromCharacter(PlayerChar);
                if (PC)
                {
                    OwnerSteamId = ArkApi::IApiUtils::GetSteamIdFromController(PC);
                }
            }
            else if (InvOwner->IsA(APrimalDinoCharacter::GetPrivateStaticClass()))
            {
                APrimalDinoCharacter* Dino = static_cast<APrimalDinoCharacter*>(InvOwner);
                TWeakObjectPtr<AShooterCharacter> RiderRef = Dino->RiderField();
                AShooterCharacter* Rider = RiderRef.Get();
                if (Rider)
                {
                    AShooterPlayerController* PC = ArkApi::GetApiUtils().FindControllerFromCharacter(Rider);
                    if (PC)
                    {
                        OwnerSteamId = ArkApi::IApiUtils::GetSteamIdFromController(PC);
                    }
                }
            }

            if (OwnerSteamId != 0)
            {
                auto harvesterIt = RecentHarvesters.find(OwnerSteamId);
                if (harvesterIt != RecentHarvesters.end())
                {
                    auto now = std::chrono::system_clock::now();
                    auto timeSinceHarvest = std::chrono::duration_cast<std::chrono::milliseconds>(now - harvesterIt->second.harvestTime).count();

                    if (timeSinceHarvest <= HARVEST_CORRELATION_WINDOW_MS)
                    {
                        isFromHarvest = true;
                    }
                }
            }
        }

        if (isFromHarvest && _this && amount > 0)
        {
            AActor* InvOwner = _this->GetOwner();

            uint64 SteamID = 0;

            if (InvOwner->IsA(APrimalDinoCharacter::StaticClass()))
            {
                APrimalDinoCharacter* Dino = static_cast<APrimalDinoCharacter*>(InvOwner);
                if (Dino->RiderField().Get())
                {
                    SteamID = GetSteamIDFromCharacter(Dino->RiderField().Get());
                }
                else if (Dino->CarryingDinoField().Get())
                {
                    APrimalDinoCharacter* CarryingDino = Dino->CarryingDinoField().Get();
                    if (CarryingDino->RiderField().Get())
                    {
                        SteamID = GetSteamIDFromCharacter(CarryingDino->RiderField().Get());
                    }
                }
            }
            else if (InvOwner->IsA(AShooterCharacter::StaticClass()))
            {
                SteamID = GetSteamIDFromCharacter(static_cast<AShooterCharacter*>(InvOwner));
            }

            if (SteamID != 0)
            {
                FString ItemBlueprint = "";
                if (ItemTemplate.uClass)
                {
                    UObject* DefaultObject = ItemTemplate.uClass->GetDefaultObject(true);
                    if (DefaultObject)
                    {
                        ItemBlueprint = ArkApi::IApiUtils::GetBlueprint(DefaultObject);
                    }
                }

                std::string blueprintStr = ItemBlueprint.ToString();
                std::transform(blueprintStr.begin(), blueprintStr.end(), blueprintStr.begin(), ::tolower);

                std::string playerName = "";
                AShooterPlayerController* PC = nullptr;
                if (InvOwner->IsA(AShooterCharacter::StaticClass()))
                {
                    PC = ArkApi::GetApiUtils().FindControllerFromCharacter(static_cast<AShooterCharacter*>(InvOwner));
                }
                else if (InvOwner->IsA(APrimalDinoCharacter::StaticClass()))
                {
                    APrimalDinoCharacter* Dino = static_cast<APrimalDinoCharacter*>(InvOwner);
                    if (Dino->RiderField().Get())
                    {
                        PC = ArkApi::GetApiUtils().FindControllerFromCharacter(Dino->RiderField().Get());
                    }
                }
                if (PC)
                {
                    FString Name = ArkApi::IApiUtils::GetCharacterName(PC);
                    playerName = Name.ToString();
                }

                std::string resourceType = "";
                if (blueprintStr.find("metal") != std::string::npos)
                    resourceType = "metal";
                else if (blueprintStr.find("flint") != std::string::npos)
                    resourceType = "flint";
                else if (blueprintStr.find("stone") != std::string::npos)
                    resourceType = "stone";
                else if (blueprintStr.find("pearl") != std::string::npos && blueprintStr.find("black") == std::string::npos)
                    resourceType = "pearl";
                else if (blueprintStr.find("blackpearl") != std::string::npos || blueprintStr.find("black_pearl") != std::string::npos)
                    resourceType = "black_pearl";
                else if (blueprintStr.find("oil") != std::string::npos)
                    resourceType = "oil";
                else if (blueprintStr.find("sulfur") != std::string::npos)
                    resourceType = "sulfur";
                else if (blueprintStr.find("fiber") != std::string::npos)
                    resourceType = "fiber";
                else if (blueprintStr.find("crystal") != std::string::npos && blueprintStr.find("element") == std::string::npos)
                    resourceType = "crystal";
                else if (blueprintStr.find("primalitemresource_wood") != std::string::npos || blueprintStr.find("resource_wood") != std::string::npos)
                    resourceType = "wood";
                else if (blueprintStr.find("primalitemresource_thatch") != std::string::npos || blueprintStr.find("resource_thatch") != std::string::npos)
                    resourceType = "thatch";
                else if (blueprintStr.find("primalitemresource_elementshard") != std::string::npos || blueprintStr.find("resource_elementshard") != std::string::npos)
                    resourceType = "element_shards";
                else if (blueprintStr.find("primalitemresource_element") != std::string::npos || blueprintStr.find("resource_element") != std::string::npos)
                    resourceType = "element";
                else if (blueprintStr.find("primalitemresource_charcoal") != std::string::npos || blueprintStr.find("resource_charcoal") != std::string::npos)
                    resourceType = "charcoal";
                else if (blueprintStr.find("primalitemresource_polymer_organic") != std::string::npos || blueprintStr.find("resource_polymer_organic") != std::string::npos || blueprintStr.find("polymer_organic") != std::string::npos)
                    resourceType = "organic_poly";
                else if (blueprintStr.find("primalitemresource_hide") != std::string::npos || blueprintStr.find("resource_hide") != std::string::npos)
                    resourceType = "hide";
                else if (blueprintStr.find("primalitemresource_keratin") != std::string::npos || blueprintStr.find("resource_keratin") != std::string::npos)
                    resourceType = "keratin";
                else if (blueprintStr.find("primalitemresource_chitin") != std::string::npos || blueprintStr.find("resource_chitin") != std::string::npos)
                    resourceType = "chitin";
                else if (blueprintStr.find("primalitemresource_megachelonshell") != std::string::npos || blueprintStr.find("resource_megachelonshell") != std::string::npos)
                    resourceType = "megachelon_shells";
                else if (blueprintStr.find("primalitemresource_obsidian") != std::string::npos || blueprintStr.find("resource_obsidian") != std::string::npos)
                    resourceType = "obsidian";

                if (!resourceType.empty())
                {
                    PlayerResourceStats[SteamID].playerName = playerName;

                    if (resourceType == "metal")
                        PlayerResourceStats[SteamID].metal += amount;
                    else if (resourceType == "flint")
                        PlayerResourceStats[SteamID].flint += amount;
                    else if (resourceType == "stone")
                        PlayerResourceStats[SteamID].stone += amount;
                    else if (resourceType == "pearl")
                        PlayerResourceStats[SteamID].pearl += amount;
                    else if (resourceType == "black_pearl")
                        PlayerResourceStats[SteamID].blackPearl += amount;
                    else if (resourceType == "oil")
                        PlayerResourceStats[SteamID].oil += amount;
                    else if (resourceType == "sulfur")
                        PlayerResourceStats[SteamID].sulfur += amount;
                    else if (resourceType == "fiber")
                        PlayerResourceStats[SteamID].fiber += amount;
                    else if (resourceType == "crystal")
                        PlayerResourceStats[SteamID].crystal += amount;
                    else if (resourceType == "wood")
                        PlayerResourceStats[SteamID].wood += amount;
                    else if (resourceType == "thatch")
                        PlayerResourceStats[SteamID].thatch += amount;
                    else if (resourceType == "element")
                        PlayerResourceStats[SteamID].element += amount;
                    else if (resourceType == "element_shards")
                        PlayerResourceStats[SteamID].elementShards += amount;
                    else if (resourceType == "charcoal")
                        PlayerResourceStats[SteamID].charcoal += amount;
                    else if (resourceType == "organic_poly")
                        PlayerResourceStats[SteamID].organicPoly += amount;
                    else if (resourceType == "hide")
                        PlayerResourceStats[SteamID].hide += amount;
                    else if (resourceType == "keratin")
                        PlayerResourceStats[SteamID].keratin += amount;
                    else if (resourceType == "chitin")
                        PlayerResourceStats[SteamID].chitin += amount;
                    else if (resourceType == "megachelon_shells")
                        PlayerResourceStats[SteamID].megachelonShells += amount;
                    else if (resourceType == "obsidian")
                        PlayerResourceStats[SteamID].obsidian += amount;
                }
            }
        }

        return UPrimalInventoryComponent_IncrementItemTemplateQuantity_original(_this, ItemTemplate, amount, bReplicateToClient, bIsBlueprint, UseSpecificItem, IncrementedItem, bRequireExactClassMatch, bIsCraftingResourceConsumption, bIsFromUseConsumption, bIsArkTributeItem, ShowHUDNotification, bDontRecalcSpoilingTime, bDontExceedMaxItems);
    }

    void SendAllResourceStatsToApi()
    {
        if (!SendToApi || ApiUrl.empty())
            return;

        if (PlayerResourceStats.empty())
            return;

        std::string mapName = GetCurrentMapName();
        FString Timestamp = GetCurrentTimestamp();
        std::string timestampStr = Timestamp.ToString();

        nlohmann::json payload;
        payload["event_type"] = "resource_harvest_batch";
        payload["map"] = mapName;
        payload["timestamp"] = timestampStr;
        payload["players"] = nlohmann::json::array();

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

            nlohmann::json playerStats;
            playerStats["player_name"] = stats.playerName;
            playerStats["steam_id"] = std::to_string(steamId);
            playerStats["metal"] = stats.metal;
            playerStats["flint"] = stats.flint;
            playerStats["stone"] = stats.stone;
            playerStats["pearl"] = stats.pearl;
            playerStats["black_pearl"] = stats.blackPearl;
            playerStats["oil"] = stats.oil;
            playerStats["sulfur"] = stats.sulfur;
            playerStats["fiber"] = stats.fiber;
            playerStats["crystal"] = stats.crystal;
            playerStats["wood"] = stats.wood;
            playerStats["thatch"] = stats.thatch;
            playerStats["element"] = stats.element;
            playerStats["element_shards"] = stats.elementShards;
            playerStats["charcoal"] = stats.charcoal;
            playerStats["organic_poly"] = stats.organicPoly;
            playerStats["hide"] = stats.hide;
            playerStats["keratin"] = stats.keratin;
            playerStats["chitin"] = stats.chitin;
            playerStats["megachelon_shells"] = stats.megachelonShells;
            playerStats["obsidian"] = stats.obsidian;

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
                if (!success)
                {
                    Log::GetLog()->warn("KillTracker: Resource stats API request failed: {}", response);
                }
            },
            jsonData,
            "application/json",
            headers
        );

        PlayerResourceStats.clear();
    }
}
