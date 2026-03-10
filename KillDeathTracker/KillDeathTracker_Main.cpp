#define _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING

#include "KillDeathTracker_Shared.h"
#include "json.hpp"
#include "Requests.h"
#include "Timer.h"
#include "API/ARK/Enums.h"

namespace KillTracker
{
    void Load()
    {
        LoadConfig();

        EnsureLogDirectoryExists();

        ArkApi::GetHooks().SetHook("AShooterCharacter.Die",
            &Hook_AShooterCharacter_Die,
            &AShooterCharacter_Die_original);

        ArkApi::GetHooks().SetHook("APrimalDinoCharacter.Die",
            &Hook_APrimalDinoCharacter_Die,
            &APrimalDinoCharacter_Die_original);

        ArkApi::GetHooks().SetHook("AShooterGameMode.PostLogin",
            &Hook_AShooterGameMode_PostLogin,
            &AShooterGameMode_PostLogin_original);

        ArkApi::GetHooks().SetHook("AShooterGameMode.Logout",
            &Hook_AShooterGameMode_Logout,
            &AShooterGameMode_Logout_original);

        ArkApi::GetHooks().SetHook("AShooterGameMode.AddNewTribe",
            &Hook_AShooterGameMode_AddNewTribe,
            &AShooterGameMode_AddNewTribe_original);

        ArkApi::GetHooks().SetHook("AShooterPlayerState.AddToTribe",
            &Hook_AShooterPlayerState_AddToTribe,
            &AShooterPlayerState_AddToTribe_original);

        ArkApi::GetHooks().SetHook("AShooterPlayerState.ClearTribe",
            &Hook_AShooterPlayerState_ClearTribe,
            &AShooterPlayerState_ClearTribe_original);

        ArkApi::GetHooks().SetHook("AShooterPlayerState.ServerRequestRenameTribe_Implementation",
            &Hook_AShooterPlayerState_ServerRequestRenameTribe,
            &AShooterPlayerState_ServerRequestRenameTribe_original);

        ArkApi::GetHooks().SetHook("AShooterWeapon.ServerStartFire_Implementation",
            &Hook_AShooterWeapon_ServerStartFire,
            &AShooterWeapon_ServerStartFire_original);

        bool weaponHookResult = ArkApi::GetHooks().SetHook("AShooterWeapon.DealDamage",
            &Hook_AShooterWeapon_DealDamage,
            &AShooterWeapon_DealDamage_original);
        if (weaponHookResult)
        {
        }
        else
        {
            Log::GetLog()->warn("KillTracker: Failed to hook AShooterWeapon.DealDamage (bone detection may not work)");
        }

        bool takeDamageHookResult = ArkApi::GetHooks().SetHook("APrimalCharacter.TakeDamage",
            &Hook_APrimalCharacter_TakeDamage,
            &APrimalCharacter_TakeDamage_original);
        if (!takeDamageHookResult)
        {
            Log::GetLog()->warn("KillTracker: Failed to hook APrimalCharacter.TakeDamage (compound bow/bola tracking may not work)");
        }

        ArkApi::GetHooks().SetHook("UPrimalHarvestingComponent.DealDirectDamage",
            &Hook_UPrimalHarvestingComponent_DealDirectDamage,
            &UPrimalHarvestingComponent_DealDirectDamage_original);

        ArkApi::GetHooks().SetHook("UPrimalInventoryComponent.NotifyItemAdded",
            &Hook_UPrimalInventoryComponent_NotifyItemAdded,
            &UPrimalInventoryComponent_NotifyItemAdded_original);

        ArkApi::GetHooks().SetHook("UPrimalHarvestingComponent.GiveHarvestResource",
            &Hook_UPrimalHarvestingComponent_GiveHarvestResource,
            &UPrimalHarvestingComponent_GiveHarvestResource_original);

        ArkApi::GetHooks().SetHook("UAnimInstance.TriggerAnimNotifies",
            &Hook_UAnimInstance_TriggerAnimNotifies,
            &UAnimInstance_TriggerAnimNotifies_original);

        ArkApi::GetHooks().SetHook("UPrimalInventoryComponent.IncrementItemTemplateQuantity",
            reinterpret_cast<LPVOID>(&Hook_UPrimalInventoryComponent_IncrementItemTemplateQuantity),
            &UPrimalInventoryComponent_IncrementItemTemplateQuantity_original);

        ArkApi::GetHooks().SetHook("AShooterGameState.StartMassTeleport",
            &Hook_AShooterGameState_StartMassTeleport,
            &AShooterGameState_StartMassTeleport_original);

        ArkApi::GetHooks().SetHook("UWorld.SpawnActor",
            &Hook_UWorld_SpawnActor,
            &UWorld_SpawnActor_original);

        ArkApi::GetHooks().SetHook("AActor.BeginPlay",
            &Hook_AActor_BeginPlay,
            &AActor_BeginPlay_original);

        if (EnableClusterIdProtection)
        {
            AShooterGameState* GameState = ArkApi::GetApiUtils().GetGameState();
            if (GameState)
            {
                RealClusterId = GameState->ClusterIdField();
                if (RealClusterId.IsEmpty())
                {
                    Log::GetLog()->warn("KillTracker: Real cluster ID is empty, will try to get it on first request");
                }
            }

            ArkApi::GetHooks().SetHook("AShooterGameSession.FindSessions",
                &Hook_AShooterGameSession_FindSessions,
                &AShooterGameSession_FindSessions_original);
        }

        API::Timer::Get().RecurringExecute(&OnBatchedEventsTimer, DataSendInterval, -1, false);
        TimerRunning = true;

        API::Timer::Get().RecurringExecute(&CheckOSDLanding, 1, -1, false);

        API::Timer::Get().RecurringExecute(&CheckPendingShots, 1, -1, false);
    }

    void Unload()
    {
        ArkApi::GetHooks().DisableHook("AShooterCharacter.Die", &Hook_AShooterCharacter_Die);
        ArkApi::GetHooks().DisableHook("APrimalDinoCharacter.Die", &Hook_APrimalDinoCharacter_Die);
        ArkApi::GetHooks().DisableHook("AShooterGameMode.PostLogin", &Hook_AShooterGameMode_PostLogin);
        ArkApi::GetHooks().DisableHook("AShooterGameMode.Logout", &Hook_AShooterGameMode_Logout);
        ArkApi::GetHooks().DisableHook("AShooterGameMode.AddNewTribe", &Hook_AShooterGameMode_AddNewTribe);
        ArkApi::GetHooks().DisableHook("AShooterPlayerState.AddToTribe", &Hook_AShooterPlayerState_AddToTribe);
        ArkApi::GetHooks().DisableHook("AShooterPlayerState.ClearTribe", &Hook_AShooterPlayerState_ClearTribe);
        ArkApi::GetHooks().DisableHook("AShooterPlayerState.ServerRequestRenameTribe_Implementation", &Hook_AShooterPlayerState_ServerRequestRenameTribe);
        ArkApi::GetHooks().DisableHook("AShooterWeapon.ServerStartFire_Implementation", &Hook_AShooterWeapon_ServerStartFire);
        ArkApi::GetHooks().DisableHook("AShooterWeapon.DealDamage", &Hook_AShooterWeapon_DealDamage);
        ArkApi::GetHooks().DisableHook("APrimalCharacter.TakeDamage", &Hook_APrimalCharacter_TakeDamage);
        ArkApi::GetHooks().DisableHook("UPrimalHarvestingComponent.DealDirectDamage", &Hook_UPrimalHarvestingComponent_DealDirectDamage);
        ArkApi::GetHooks().DisableHook("UPrimalInventoryComponent.NotifyItemAdded", &Hook_UPrimalInventoryComponent_NotifyItemAdded);
        ArkApi::GetHooks().DisableHook("UPrimalHarvestingComponent.GiveHarvestResource", &Hook_UPrimalHarvestingComponent_GiveHarvestResource);
        ArkApi::GetHooks().DisableHook("UAnimInstance.TriggerAnimNotifies", &Hook_UAnimInstance_TriggerAnimNotifies);
        ArkApi::GetHooks().DisableHook("UPrimalInventoryComponent.IncrementItemTemplateQuantity", reinterpret_cast<LPVOID>(&Hook_UPrimalInventoryComponent_IncrementItemTemplateQuantity));
        ArkApi::GetHooks().DisableHook("AShooterGameState.StartMassTeleport", &Hook_AShooterGameState_StartMassTeleport);
        ArkApi::GetHooks().DisableHook("UWorld.SpawnActor", &Hook_UWorld_SpawnActor);
        ArkApi::GetHooks().DisableHook("AActor.BeginPlay", &Hook_AActor_BeginPlay);

        if (EnableClusterIdProtection)
        {
            ArkApi::GetHooks().DisableHook("AShooterGameSession.FindSessions", &Hook_AShooterGameSession_FindSessions);
        }

        PlayerShotStats.clear();
        RecentHarvesters.clear();
        PlayerResourceStats.clear();
        PlayersInBossFight.clear();
        PlayerBodyPartStats.clear();
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

extern "C" __declspec(dllexport) void Plugin_Init()
{
    Log::Get().Init("KillTracker");
    KillTracker::Load();
}

extern "C" __declspec(dllexport) void Plugin_Unload()
{
    KillTracker::Unload();
}
