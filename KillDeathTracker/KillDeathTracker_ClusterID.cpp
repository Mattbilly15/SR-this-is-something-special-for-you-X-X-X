#define _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING

#include "KillDeathTracker_Shared.h"
#include "json.hpp"
#include "Requests.h"
#include "Timer.h"
#include "API/ARK/Enums.h"

namespace KillTracker
{
    // DECLARE_HOOK macros moved to KillDeathTracker_Shared.h

    bool ValidateClusterIdPassword(const FString& providedPassword)
    {
        if (!EnableClusterIdProtection)
            return true;

        if (ClusterIdPassword.empty())
            return false;

        std::string providedStr = providedPassword.ToString();
        return (providedStr == ClusterIdPassword);
    }

    FString GetClusterIdForRequest(const FString& providedPassword)
    {
        if (ValidateClusterIdPassword(providedPassword))
        {
            return RealClusterId;
        }
        else
        {
            if (EnableSpoofing)
            {
                return FString(SpoofedClusterId.c_str());
            }
            else
            {
                return FString("");
            }
        }
    }

    void Hook_AShooterGameSession_FindSessions(AShooterGameSession* _this, TSharedPtr<FUniqueNetId, 0> UserId,
                                               FName SessionName, bool bIsLAN, bool bIsPresence,
                                               bool bRecreateSearchSettings, EListSessionStatus::Type FindType,
                                               bool bQueryNotFullSessions, bool bPasswordServers,
                                               const wchar_t* ServerName, FString ClusterId, FString AtlasId,
                                               FString ServerId, FString AuthListURL)
    {
        if (!EnableClusterIdProtection)
        {
            AShooterGameSession_FindSessions_original(_this, UserId, SessionName, bIsLAN, bIsPresence,
                                                      bRecreateSearchSettings, FindType, bQueryNotFullSessions,
                                                      bPasswordServers, ServerName, ClusterId, AtlasId, ServerId, AuthListURL);
            return;
        }

        if (RealClusterId.IsEmpty())
        {
            AShooterGameState* GameState = ArkApi::GetApiUtils().GetGameState();
            if (GameState)
            {
                RealClusterId = GameState->ClusterIdField();
            }
        }

        FString providedPassword = "";
        FString modifiedClusterId = ClusterId;

        std::string authUrlStr = AuthListURL.ToString();
        if (authUrlStr.find("PWD:") == 0)
        {
            std::string pwdStr = authUrlStr.substr(4);
            providedPassword = FString(pwdStr.c_str());
        }

        if (!providedPassword.IsEmpty())
        {
            modifiedClusterId = GetClusterIdForRequest(providedPassword);
        }
        else
        {
            if (EnableSpoofing)
            {
                modifiedClusterId = FString(SpoofedClusterId.c_str());
            }
            else
            {
                modifiedClusterId = FString("");
            }
        }

        if (EnableClusterIdProtection && !ClusterIdPassword.empty())
        {
            std::string clusterIdStr = modifiedClusterId.ToString();
            std::string realClusterIdStr = RealClusterId.ToString();
            bool isAuthenticated = (clusterIdStr == realClusterIdStr);
        }

        AShooterGameSession_FindSessions_original(_this, UserId, SessionName, bIsLAN, bIsPresence,
                                                   bRecreateSearchSettings, FindType, bQueryNotFullSessions,
                                                   bPasswordServers, ServerName, modifiedClusterId, AtlasId, ServerId, AuthListURL);
    }
}
