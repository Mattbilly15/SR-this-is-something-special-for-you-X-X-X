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

    void ProcessOSDSpawn(AActor* actor, const FVector& location)
    {
        if (!actor)
            return;

        FString Timestamp = GetCurrentTimestamp();
        std::string timestampStr = Timestamp.ToString();

        FString Blueprint = ArkApi::IApiUtils::GetBlueprint(actor);
        std::string blueprintStr = Blueprint.ToString();
        std::transform(blueprintStr.begin(), blueprintStr.end(), blueprintStr.begin(), ::tolower);

        std::string typeStr = "Unknown";

        std::string originalBlueprint = Blueprint.ToString();
        std::string originalBlueprintLower = originalBlueprint;
        std::transform(originalBlueprintLower.begin(), originalBlueprintLower.end(), originalBlueprintLower.begin(), ::tolower);

        if (originalBlueprintLower.find("legendary") != std::string::npos)
            typeStr = "Legendary";
        else if (originalBlueprintLower.find("hard") != std::string::npos)
            typeStr = "Hard";

        if (originalBlueprintLower.find("red") != std::string::npos)
        {
            if (typeStr == "Hard")
                typeStr = "Red (Hard)";
            else if (typeStr == "Legendary")
                typeStr = "Red (Legendary)";
            else
                typeStr = "Red";
        }
        else if (originalBlueprintLower.find("purple") != std::string::npos)
        {
            if (typeStr == "Hard")
                typeStr = "Purple (Hard)";
            else if (typeStr == "Legendary")
                typeStr = "Purple (Legendary)";
            else
                typeStr = "Purple";
        }

        if (typeStr == "Unknown")
        {
            return;
        }

        OSDTrackingInfo trackingInfo;
        trackingInfo.actor = actor;
        trackingInfo.lastLocation = location;
        trackingInfo.typeStr = typeStr;
        trackingInfo.spawnTime = std::chrono::system_clock::now();
        trackingInfo.hasLanded = false;
        trackingInfo.stationaryCount = 0;
        OSDTrackingMap[actor] = trackingInfo;

        if (LogToFile)
        {
            EnsureLogDirectoryExists();
            std::string logPathStr = LogFilePath.ToString();
            std::string filePath = logPathStr + "osd_spawns.log";
            std::ofstream file(filePath, std::ios::app);
            if (file.is_open())
            {
                file << "[" << timestampStr << "] " << typeStr << " OSD spawned at X: "
                     << location.X << ", Y: " << location.Y << ", Z: " << location.Z << "\n";
                file.close();
            }
        }
    }

    void CheckOSDLanding()
    {
        if (OSDTrackingMap.empty())
            return;

        auto it = OSDTrackingMap.begin();
        while (it != OSDTrackingMap.end())
        {
            AActor* actor = it->first;
            OSDTrackingInfo& info = it->second;

            if (!actor)
            {
                it = OSDTrackingMap.erase(it);
                continue;
            }

            FVector currentLocation(0, 0, 0);
            try
            {
                USceneComponent* RootComp = actor->RootComponentField();
                if (RootComp)
                {
                    RootComp->GetWorldLocation(&currentLocation);
                }
            }
            catch (...)
            {
                Log::GetLog()->warn("KillTracker: Exception accessing OSD actor location, removing from tracking");
                it = OSDTrackingMap.erase(it);
                continue;
            }

            float distanceMoved = FVector::Dist(info.lastLocation, currentLocation);

            if (distanceMoved < 50.0f)
            {
                info.stationaryCount++;

                if (info.stationaryCount >= 3 && !info.hasLanded)
                {
                    info.hasLanded = true;

                    if (EnableOSDSpawnBroadcast)
                    {
                        try
                        {
                            FString TypeFStr = FString(info.typeStr.c_str());
                            if (ArkApi::GetApiUtils().GetStatus() == ArkApi::ServerStatus::Ready)
                            {
                                ArkApi::MapCoords coords = ArkApi::GetApiUtils().FVectorToCoords(currentLocation);
                            }
                        }
                        catch (...)
                        {
                            Log::GetLog()->error("KillTracker: Exception broadcasting OSD landing coordinates");
                        }
                    }

                    if (SendToApi && !ApiUrl.empty())
                    {
                        try
                        {
                            FString Timestamp = GetCurrentTimestamp();
                            QueueSpawnEvent("osd_spawn", info.typeStr, currentLocation, Timestamp.ToString());
                        }
                        catch (...)
                        {
                            Log::GetLog()->error("KillTracker: Exception queuing OSD spawn event");
                        }
                    }

                    if (LogToFile)
                    {
                        EnsureLogDirectoryExists();
                        std::string logPathStr = LogFilePath.ToString();
                        std::string filePath = logPathStr + "osd_spawns.log";
                        FString Timestamp = GetCurrentTimestamp();
                        std::ofstream file(filePath, std::ios::app);
                        if (file.is_open())
                        {
                            file << "[" << Timestamp.ToString() << "] " << info.typeStr
                                 << " OSD landed at X: " << currentLocation.X
                                 << ", Y: " << currentLocation.Y
                                 << ", Z: " << currentLocation.Z << "\n";
                            file.close();
                        }
                    }

                    it = OSDTrackingMap.erase(it);
                    continue;
                }
            }
            else
            {
                info.stationaryCount = 0;
                info.lastLocation = currentLocation;
            }

            auto now = std::chrono::system_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - info.spawnTime).count();
            if (elapsed > 60)
            {
                Log::GetLog()->warn("KillTracker: {} OSD tracking timeout after 60 seconds, removing from tracking", info.typeStr);
                it = OSDTrackingMap.erase(it);
                continue;
            }

            ++it;
        }
    }

    AActor* Hook_UWorld_SpawnActor(UWorld* world, UClass* clazz, FTransform const& transform, FActorSpawnParameters& params)
    {
        return UWorld_SpawnActor_original(world, clazz, transform, params);
    }

    void Hook_AActor_BeginPlay(AActor* _this)
    {
        AActor_BeginPlay_original(_this);

        ProcessProjectileBeginPlay(_this);

        if (!_this)
            return;

        FString Blueprint = ArkApi::IApiUtils::GetBlueprint(_this);
        if (Blueprint.IsEmpty())
            return;

        std::string blueprintStr = Blueprint.ToString();
        std::transform(blueprintStr.begin(), blueprintStr.end(), blueprintStr.begin(), ::tolower);

        bool isOSD = ((blueprintStr.find("supplycrate") != std::string::npos && blueprintStr.find("horde") != std::string::npos) ||
                      (blueprintStr.find("osdeventmap") != std::string::npos && blueprintStr.find("supplycrates") != std::string::npos) ||
                      blueprintStr.find("orbitalsupplydrop") != std::string::npos ||
                      blueprintStr.find("supplydrop") != std::string::npos ||
                      blueprintStr.find("osd") != std::string::npos ||
                      blueprintStr.find("orbital_supply") != std::string::npos ||
                      blueprintStr.find("supply_drop") != std::string::npos);

        bool isElementNodeMatinee = (blueprintStr.find("elementnodematinee") != std::string::npos);

        bool isElementalVein = ((blueprintStr.find("elementnode") != std::string::npos ||
                                 blueprintStr.find("elementvein") != std::string::npos ||
                                 blueprintStr.find("elementalvein") != std::string::npos ||
                                 blueprintStr.find("element_vein") != std::string::npos) &&
                                !isElementNodeMatinee);

        if (isOSD || isElementalVein)
        {
            FVector Location(0, 0, 0);
            USceneComponent* RootComp = _this->RootComponentField();
            if (RootComp)
            {
                RootComp->GetWorldLocation(&Location);
            }
            else
            {
                Log::GetLog()->warn("KillTracker: Could not get root component for Elemental Vein/OSD actor");
            }

            if (isElementalVein)
            {
                std::string originalBlueprint = Blueprint.ToString();
                std::string originalBlueprintLower = originalBlueprint;
                std::transform(originalBlueprintLower.begin(), originalBlueprintLower.end(), originalBlueprintLower.begin(), ::tolower);

                std::string veinType = "Unknown";
                if (originalBlueprintLower.find("hard") != std::string::npos)
                    veinType = "Hard";
                else if (originalBlueprintLower.find("medium") != std::string::npos)
                    veinType = "Medium";
                else if (originalBlueprintLower.find("easy") != std::string::npos)
                    veinType = "Easy";

                FString TimestampFStr = GetCurrentTimestamp();
                std::string timestampStr = TimestampFStr.ToString();
                QueueSpawnEvent("elemental_vein", veinType, Location, timestampStr);
            }

            FString Timestamp = GetCurrentTimestamp();
            std::string timestampStr = Timestamp.ToString();

            std::string typeStr = "Unknown";

            if (isOSD)
            {
                std::string originalBlueprint = Blueprint.ToString();
                std::string originalBlueprintLower = originalBlueprint;
                std::transform(originalBlueprintLower.begin(), originalBlueprintLower.end(), originalBlueprintLower.begin(), ::tolower);

                if (originalBlueprintLower.find("legendary") != std::string::npos)
                    typeStr = "Legendary";
                else if (originalBlueprintLower.find("hard") != std::string::npos)
                    typeStr = "Hard";

                if (originalBlueprintLower.find("red") != std::string::npos)
                {
                    if (typeStr == "Hard")
                        typeStr = "Red (Hard)";
                    else if (typeStr == "Legendary")
                        typeStr = "Red (Legendary)";
                    else
                        typeStr = "Red";
                }
                else if (originalBlueprintLower.find("purple") != std::string::npos)
                {
                    if (typeStr == "Hard")
                        typeStr = "Purple (Hard)";
                    else if (typeStr == "Legendary")
                        typeStr = "Purple (Legendary)";
                    else
                        typeStr = "Purple";
                }
            }
            else if (isElementalVein)
            {
                std::string originalBlueprint = Blueprint.ToString();
                std::string originalBlueprintLower = originalBlueprint;
                std::transform(originalBlueprintLower.begin(), originalBlueprintLower.end(), originalBlueprintLower.begin(), ::tolower);

                if (originalBlueprintLower.find("hard") != std::string::npos)
                    typeStr = "Hard";
                else if (originalBlueprintLower.find("medium") != std::string::npos)
                    typeStr = "Medium";
                else if (originalBlueprintLower.find("easy") != std::string::npos)
                    typeStr = "Easy";
                else
                {
                    if (originalBlueprintLower.find("rich") != std::string::npos)
                        typeStr = "Rich";
                    else if (originalBlueprintLower.find("normal") != std::string::npos ||
                             originalBlueprintLower.find("standard") != std::string::npos)
                        typeStr = "Normal";
                    else if (originalBlueprintLower.find("poor") != std::string::npos)
                        typeStr = "Poor";
                    else
                        typeStr = "Unknown";
                }
            }

            if (isOSD)
            {
                ProcessOSDSpawn(_this, Location);
            }
            else
            {
                if (typeStr == "Unknown")
                {
                    Log::GetLog()->warn("KillTracker: Skipping Unknown Elemental Vein type - Blueprint: {}", Blueprint.ToString());
                    return;
                }

                if (EnableElementalVeinSpawnBroadcast)
                {
                    try
                    {
                        FString TypeFStr = FString(typeStr.c_str());

                        bool useLatLon = false;
                        ArkApi::MapCoords coords;

                        if (ArkApi::GetApiUtils().GetStatus() == ArkApi::ServerStatus::Ready)
                        {
                            try
                            {
                                coords = ArkApi::GetApiUtils().FVectorToCoords(Location);
                                useLatLon = true;
                            }
                            catch (...)
                            {
                                Log::GetLog()->warn("KillTracker: Failed to convert Elemental Vein coordinates, using SPI");
                                useLatLon = false;
                            }
                        }
                    }
                    catch (...)
                    {
                        Log::GetLog()->error("KillTracker: Exception broadcasting Elemental Vein spawn message");
                    }
                }
            }

            if (SendToApi && !ApiUrl.empty() && isElementalVein && typeStr != "Unknown")
            {
                QueueSpawnEvent("elemental_vein_spawn", typeStr, Location, timestampStr);
            }

            if (LogToFile && isElementalVein && typeStr != "Unknown")
            {
                EnsureLogDirectoryExists();
                std::string logPathStr = LogFilePath.ToString();
                std::string filePath = logPathStr + "elemental_vein_spawns.log";
                std::ofstream file(filePath, std::ios::app);
                if (file.is_open())
                {
                    file << "[" << timestampStr << "] " << typeStr << " Elemental Vein spawned at X: "
                         << Location.X << ", Y: " << Location.Y << ", Z: " << Location.Z << "\n";
                    file.close();
                }
            }
        }
    }
}
