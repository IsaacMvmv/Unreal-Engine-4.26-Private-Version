// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"
#include "UObject/WeakObjectPtr.h"

#include "Interfaces/ITargetPlatformModule.h"

#include "LinuxTargetSettings.h"
#include "LinuxTargetDevice.h"
#include "LinuxTargetPlatform.h"

#include "ISettingsModule.h"


#define LOCTEXT_NAMESPACE "FLinuxTargetPlatformModule"


/**
 * Holds the target platform singleton.
 */
static ITargetPlatform* Singleton = NULL;


/**
 * Module for the Linux target platform.
 */
class FLinuxTargetPlatformModule
	: public ITargetPlatformModule
{
public:

	/** Destructor. */
	~FLinuxTargetPlatformModule( )
	{
		Singleton = NULL;
	}

public:
	
	// ITargetPlatformModule interface

	virtual ITargetPlatform* GetTargetPlatform( ) override
	{
		if (Singleton == NULL && TLinuxTargetPlatform<FLinuxPlatformProperties<true, false, false, false> >::IsUsable())
		{
			Singleton = new TLinuxTargetPlatform<FLinuxPlatformProperties<true, false, false, false> >();
		}
		
		return Singleton;
	}

public:

	// IModuleInterface interface

	virtual void StartupModule() override
	{
		TargetSettings = NewObject<ULinuxTargetSettings>(GetTransientPackage(), "LinuxTargetSettings", RF_Standalone);

		// We need to manually load the config properties here, as this module is loaded before the UObject system is setup to do this
		GConfig->GetArray(TEXT("/Script/LinuxTargetPlatform.LinuxTargetSettings"), TEXT("TargetedRHIs"), TargetSettings->TargetedRHIs, GEngineIni);
		TargetSettings->AddToRoot();
		
        if (!GConfig->GetBool(TEXT("/Script/LinuxTargetPlatform.LinuxTargetSettings"), TEXT("bCookDXTTextures"), TargetSettings->bCookDXTTextures, GEngineIni))
        {
                TargetSettings->bCookDXTTextures = true;
        }

        if (!GConfig->GetBool(TEXT("/Script/LinuxTargetPlatform.LinuxTargetSettings"), TEXT("bCookBCTextures"), TargetSettings->bCookBCTextures, GEngineIni))
        {
                TargetSettings->bCookBCTextures = true;
        }

		if (!GConfig->GetBool(TEXT("/Script/LinuxTargetPlatform.LinuxTargetSettings"), TEXT("bCookETC2Textures"), TargetSettings->bCookETC2Textures, GEngineIni))
        {
                TargetSettings->bCookETC2Textures = true;
        }

		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->RegisterSettings("Project", "Platforms", "Linux",
				LOCTEXT("TargetSettingsName", "Linux"),
				LOCTEXT("TargetSettingsDescription", "Settings for Linux target platform"),
				TargetSettings
			);
		}
	}

	virtual void ShutdownModule() override
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->UnregisterSettings("Project", "Platforms", "Linux");
		}

		if (!GExitPurge)
		{
			// If we're in exit purge, this object has already been destroyed
			TargetSettings->RemoveFromRoot();
		}
		else
		{
			TargetSettings = NULL;
		}
	}

private:

	/** Holds the target settings. */
	ULinuxTargetSettings* TargetSettings;
};


#undef LOCTEXT_NAMESPACE


IMPLEMENT_MODULE( FLinuxTargetPlatformModule, LinuxTargetPlatform);
