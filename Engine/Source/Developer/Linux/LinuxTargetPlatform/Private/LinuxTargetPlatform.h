// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LinuxTargetPlatform.h: Declares the FLinuxTargetPlatform class.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/TargetDeviceId.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "Interfaces/ITargetPlatform.h"
#include "Common/TargetPlatformBase.h"


#if WITH_ENGINE
#include "AudioCompressionSettings.h"
#include "Sound/SoundWave.h"
#include "StaticMeshResources.h"
#endif // WITH_ENGINE
#include "Interfaces/IProjectManager.h"
#include "InstalledPlatformInfo.h"
#include "LinuxTargetDevice.h"
#include "Linux/LinuxPlatformProperties.h"

#define LOCTEXT_NAMESPACE "TLinuxTargetPlatform"

class UTextureLODSettings;

namespace LinuxTextureFormats
{
       static FName NameETC2RGB(TEXT("ETC2_RGB"));
       static FName NameETC2RGBA(TEXT("ETC2_RGBA"));
       static FName NameBGRA8(TEXT("BGRA8"));
}

/**
 * Template for Linux target platforms
 */
template<typename TProperties>
class TLinuxTargetPlatform
	: public TTargetPlatformBase<TProperties>
{
public:

	typedef TTargetPlatformBase<TProperties> TSuper;

	/**
	 * Default constructor.
	 */
	TLinuxTargetPlatform( )
#if WITH_ENGINE
		: bChangingDeviceConfig(false)
#endif // WITH_ENGINE
	{
#if PLATFORM_LINUX
		if (!TProperties::IsAArch64())
		{
			// only add local device if actually running on Linux
			LocalDevice = MakeShareable(new FLinuxTargetDevice(*this, FPlatformProcess::ComputerName(), nullptr));
		}
#endif

#if WITH_ENGINE
		FConfigCacheIni::LoadLocalIniFile(EngineSettings, TEXT("Engine"), true, *this->PlatformName());
		TextureLODSettings = nullptr;
		StaticMeshLODSettings.Initialize(EngineSettings);

		InitDevicesFromConfig();

		// Get the Target RHIs for this platform, we do not always want all those that are supported.
		TArray<FName> TargetedShaderFormats;
		GetAllTargetedShaderFormats(TargetedShaderFormats);

		// If we are targeting ES 2.0/3.1, we also must cook encoded HDR reflection captures
		static FName NAME_SF_VULKAN_ES31(TEXT("SF_VULKAN_ES31"));
		static FName NAME_OPENGL_150_ES3_1(TEXT("GLSL_150_ES31"));
		bRequiresEncodedHDRReflectionCaptures = TargetedShaderFormats.Contains(NAME_SF_VULKAN_ES31)
			|| TargetedShaderFormats.Contains(NAME_OPENGL_150_ES3_1);
#endif // WITH_ENGINE
	}


public:

	//~ Begin ITargetPlatform Interface

	virtual void EnableDeviceCheck(bool OnOff) override {}

	virtual bool AddDevice(const FString& DeviceName, bool bDefault) override
	{
		return AddDevice(DeviceName, TEXT(""), TEXT(""), TEXT(""), bDefault);
	}

	virtual bool AddDevice(const FString& DeviceName, const FString& DeviceUserFriendlyName, const FString& Username, const FString& Password, bool bDefault) override
	{
		FLinuxTargetDevicePtr& Device = Devices.FindOrAdd(DeviceName);

		if (Device.IsValid())
		{
			// do not allow duplicates
			return false;
		}

		Device = MakeShareable(new FLinuxTargetDevice(*this, DeviceName,
#if WITH_ENGINE
			[&]() { SaveDevicesToConfig(); }));
		SaveDevicesToConfig();	// this will do the right thing even if AddDevice() was called from InitDevicesFromConfig
#else
			nullptr));
#endif // WITH_ENGINE

		if (!Username.IsEmpty() || !Password.IsEmpty())
		{
			Device->SetUserCredentials(Username, Password);
		}

		DeviceDiscoveredEvent.Broadcast(Device.ToSharedRef());
		return true;
	}


	virtual void GetAllDevices( TArray<ITargetDevicePtr>& OutDevices ) const override
	{
		// TODO: ping all the machines in a local segment and/or try to connect to port 22 of those that respond
		OutDevices.Reset();
		if (LocalDevice.IsValid())
		{
			OutDevices.Add(LocalDevice);
		}

		for (const auto & DeviceIter : Devices)
		{
			OutDevices.Add(DeviceIter.Value);
		}
	}

	virtual bool GenerateStreamingInstallManifest(const TMultiMap<FString, int32>& PakchunkMap, const TSet<int32>& PakchunkIndicesInUse) const override
	{
		return true;
	}

	virtual ITargetDevicePtr GetDefaultDevice( ) const override
	{
		if (LocalDevice.IsValid())
		{
			return LocalDevice;
		}

		return nullptr;
	}

	virtual ITargetDevicePtr GetDevice( const FTargetDeviceId& DeviceId ) override
	{
		if (LocalDevice.IsValid() && (DeviceId == LocalDevice->GetId()))
		{
			return LocalDevice;
		}

		for (const auto & DeviceIter : Devices)
		{
			if (DeviceId == DeviceIter.Value->GetId())
			{
				return DeviceIter.Value;
			}
		}

		return nullptr;
	}

	virtual bool IsRunningPlatform( ) const override
	{
		// Must be Linux platform as editor for this to be considered a running platform
		return PLATFORM_LINUX && !UE_SERVER && !UE_GAME && WITH_EDITOR && TProperties::HasEditorOnlyData();
	}

	virtual bool SupportsFeature(ETargetPlatformFeatures Feature) const override
	{
		if (Feature == ETargetPlatformFeatures::UserCredentials || Feature == ETargetPlatformFeatures::Packaging)
		{
			return true;
		}

		return TSuper::SupportsFeature(Feature);
	}

	virtual bool IsSdkInstalled(bool bProjectHasCode, FString& OutDocumentationPath) const override
	{
		if (!PLATFORM_LINUX)
		{
			// check for LINUX_MULTIARCH_ROOT or for legacy LINUX_ROOT when targeting Linux from Win/Mac

			// proceed with any value for MULTIARCH root, because checking exact architecture is not possible at this point
			FString ToolchainMultiarchRoot = FPlatformMisc::GetEnvironmentVariable(TEXT("LINUX_MULTIARCH_ROOT"));
			if (ToolchainMultiarchRoot.Len() > 0 && FPaths::DirectoryExists(ToolchainMultiarchRoot))
			{
				return true;
			}

			// else check for legacy LINUX_ROOT
			FString ToolchainCompiler = FPlatformMisc::GetEnvironmentVariable(TEXT("LINUX_ROOT"));
			if (PLATFORM_WINDOWS)
			{
				ToolchainCompiler += "/bin/clang++.exe";
			}
			else if (PLATFORM_MAC)
			{
				ToolchainCompiler += "/bin/clang++";
			}
			else
			{
				checkf(false, TEXT("Unable to target Linux on an unknown platform."));
				return false;
			}

			return FPaths::FileExists(ToolchainCompiler);
		}

		return true;
	}

	virtual int32 CheckRequirements(bool bProjectHasCode, EBuildConfiguration Configuration, bool bRequiresAssetNativization, FString& OutTutorialPath, FString& OutDocumentationPath, FText& CustomizedLogMessage) const override
	{
		int32 ReadyToBuild = TSuper::CheckRequirements(bProjectHasCode, Configuration, bRequiresAssetNativization, OutTutorialPath, OutDocumentationPath, CustomizedLogMessage);

		// do not support code/plugins in Installed builds if the required libs aren't bundled (on Windows/Mac)
		if (!PLATFORM_LINUX && !FInstalledPlatformInfo::Get().IsValidPlatform(TSuper::GetPlatformInfo().BinaryFolderName, EProjectType::Code))
		{
			if (bProjectHasCode)
			{
				ReadyToBuild |= ETargetPlatformReadyStatus::CodeUnsupported;
			}

			FText Reason;
			if (this->RequiresTempTarget(bProjectHasCode, Configuration, bRequiresAssetNativization, Reason))
			{
				ReadyToBuild |= ETargetPlatformReadyStatus::PluginsUnsupported;
			}
		}

		return ReadyToBuild;
	}


#if WITH_ENGINE
	virtual void GetReflectionCaptureFormats(TArray<FName>& OutFormats) const override
	{
		if (bRequiresEncodedHDRReflectionCaptures)
		{
			OutFormats.Add(FName(TEXT("EncodedHDR")));
		}

		OutFormats.Add(FName(TEXT("FullHDR")));
	}

	virtual void GetAllPossibleShaderFormats( TArray<FName>& OutFormats ) const override
	{
		// no shaders needed for dedicated server target
		if (!TProperties::IsServerOnly())
		{
			static FName NAME_VULKAN_SM5(TEXT("SF_VULKAN_SM5"));
			static FName NAME_VULKAN_ES31(TEXT("SF_VULKAN_ES31"));

			OutFormats.AddUnique(NAME_VULKAN_SM5);
			OutFormats.AddUnique(NAME_VULKAN_ES31);
		}
	}

	virtual void GetAllTargetedShaderFormats( TArray<FName>& OutFormats ) const override
	{
		// Get the Target RHIs for this platform, we do not always want all those that are supported. (reload in case user changed in the editor)
		TArray<FString>TargetedShaderFormats;
		GConfig->GetArray(TEXT("/Script/LinuxTargetPlatform.LinuxTargetSettings"), TEXT("TargetedRHIs"), TargetedShaderFormats, GEngineIni);

		// Gather the list of Target RHIs and filter out any that may be invalid.
		TArray<FName> PossibleShaderFormats;
		GetAllPossibleShaderFormats(PossibleShaderFormats);

		for (int32 ShaderFormatIdx = TargetedShaderFormats.Num() - 1; ShaderFormatIdx >= 0; ShaderFormatIdx--)
		{
			FString ShaderFormat = TargetedShaderFormats[ShaderFormatIdx];
			if (PossibleShaderFormats.Contains(FName(*ShaderFormat)) == false)
			{
				TargetedShaderFormats.RemoveAt(ShaderFormatIdx);
			}
		}

		for(const FString& ShaderFormat : TargetedShaderFormats)
		{
			OutFormats.AddUnique(FName(*ShaderFormat));
		}
	}
	virtual const class FStaticMeshLODSettings& GetStaticMeshLODSettings( ) const override
	{
		return StaticMeshLODSettings;
	}


	virtual void GetTextureFormats( const UTexture* InTexture, TArray< TArray<FName> >& OutFormats) const override
	{
		if (!TProperties::IsServerOnly())
		{
			// just use the standard texture format name for this texture
			GetDefaultTextureFormatNamePerLayer(OutFormats.AddDefaulted_GetRef(), this, InTexture, EngineSettings, true);
            bool bCookDXTTextures = true;
            GConfig->GetBool(TEXT("/Script/LinuxTargetPlatform.LinuxTargetSettings"), TEXT("bCookDXTTextures"), bCookDXTTextures, GEngineIni);
            bool bCookBCTextures = true;
            GConfig->GetBool(TEXT("/Script/LinuxTargetPlatform.LinuxTargetSettings"), TEXT("bCookBCTextures"), bCookBCTextures, GEngineIni);
            bool bCookETC2Textures = false;
            GConfig->GetBool(TEXT("/Script/LinuxTargetPlatform.LinuxTargetSettings"), TEXT("bCookETC2Textures"), bCookETC2Textures, GEngineIni);

            for (TArray<FName>& LayerNames : OutFormats)
            {
                    for (int32 NameIndex = LayerNames.Num() - 1; NameIndex >= 0; NameIndex--)
                    {
                            const FString Name = LayerNames[NameIndex].ToString();
                            if (Name.Contains("DXT"))
                            {
                                    if (!bCookDXTTextures)
                                    {
                                            if (bCookETC2Textures)
                                            {
                                                    if (Name == "DXT1")
                                                    {
                                                            LayerNames[NameIndex] = LinuxTextureFormats::NameETC2RGB;
                                                    }
                                                     else
                                                     {
                                                             LayerNames[NameIndex] = LinuxTextureFormats::NameETC2RGBA;
                                                     }
                                             }
                                             else
                                             {
                                                      LayerNames[NameIndex] = LinuxTextureFormats::NameBGRA8;
                                             }
                                    }
                            }
                            else if (Name.StartsWith("BC"))
                            {
                                    if (!bCookBCTextures)
                                    {
                                            if (bCookETC2Textures)
                                            {
                                                    LayerNames[NameIndex] = LinuxTextureFormats::NameETC2RGB;
                                            }
                                            else
                                            {
                                                    LayerNames[NameIndex] = LinuxTextureFormats::NameBGRA8;
                                            }
                                    }
                             }
                    }
            }
		}
	}


	virtual void GetAllTextureFormats(TArray<FName>& OutFormats) const override
	{
		if (!TProperties::IsServerOnly())
		{
			// just use the standard texture format name for this texture
			GetAllDefaultTextureFormats(this, OutFormats, true);
			bool bCookDXTTextures = true;
			GConfig->GetBool(TEXT("/Script/LinuxTargetPlatform.LinuxTargetSettings"), TEXT("bCookDXTTextures"), bCookDXTTextures, GEngineIni);
			bool bCookBCTextures = true;
			GConfig->GetBool(TEXT("/Script/LinuxTargetPlatform.LinuxTargetSettings"), TEXT("bCookBCTextures"), bCookBCTextures, GEngineIni);
			bool bCookETC2Textures = false;
			GConfig->GetBool(TEXT("/Script/LinuxTargetPlatform.LinuxTargetSettings"), TEXT("bCookETC2Textures"), bCookETC2Textures, GEngineIni);

			for (int32 NameIndex = OutFormats.Num() - 1; NameIndex >= 0; NameIndex--)
			{
					const FString Name = OutFormats[NameIndex].ToString();
                    if (Name.Contains("DXT"))
                    {
                               if (!bCookDXTTextures)
                               {
                                        if (bCookETC2Textures)
                                        {
                                                if (Name == "DXT1")
                                                {
                                                        OutFormats[NameIndex] = LinuxTextureFormats::NameETC2RGB;
                                                }
                                                else
                                                {
                                                        OutFormats[NameIndex] = LinuxTextureFormats::NameETC2RGBA;
                                                }
                                        }
                                        else
                                        {
                                                OutFormats.RemoveAt(NameIndex);
                                        }

                                }

                    }
                    else if (Name.StartsWith("BC"))
                    {
                             if (!bCookBCTextures)
                             {
                                     if (bCookETC2Textures)
                                     {
                                            OutFormats[NameIndex] = LinuxTextureFormats::NameETC2RGB;
                                     }
                                     else
                                     {
                                             OutFormats.RemoveAt(NameIndex);
                                     }
                              }
                    }
            }
		}
	}

	virtual const UTextureLODSettings& GetTextureLODSettings() const override
	{
		return *TextureLODSettings;
	}

	virtual void RegisterTextureLODSettings(const UTextureLODSettings* InTextureLODSettings) override
	{
		TextureLODSettings = InTextureLODSettings;
	}

	virtual FName GetWaveFormat( const class USoundWave* Wave ) const override
	{
		static const FName NAME_ADPCM(TEXT("ADPCM"));
		static const FName NAME_OGG(TEXT("OGG"));
		static const FName NAME_OPUS(TEXT("OPUS"));

		if (Wave->IsSeekableStreaming())
		{
			return NAME_ADPCM;
		}

		if (Wave->IsStreaming(*this->IniPlatformName()))
		{
			return NAME_OPUS;
		}

		return NAME_OGG;
	}

	virtual void GetAllWaveFormats(TArray<FName>& OutFormats) const override
	{
		static const FName NAME_ADPCM(TEXT("ADPCM"));
		static const FName NAME_OGG(TEXT("OGG"));
		static const FName NAME_OPUS(TEXT("OPUS"));

		OutFormats.Add(NAME_ADPCM);
		OutFormats.Add(NAME_OGG);
		OutFormats.Add(NAME_OPUS);
	}

#endif //WITH_ENGINE

	virtual bool SupportsVariants() const override
	{
		return true;
	}

	virtual FText GetVariantDisplayName() const override
	{
		if (TProperties::IsServerOnly())
		{
			return LOCTEXT("LinuxServerVariantTitle", "Dedicated Server");
		}

		if (TProperties::HasEditorOnlyData())
		{
			return LOCTEXT("LinuxClientEditorDataVariantTitle", "Client with Editor Data");
		}

		if (TProperties::IsClientOnly())
		{
			return LOCTEXT("LinuxClientOnlyVariantTitle", "Client only");
		}

		return LOCTEXT("LinuxClientVariantTitle", "Client");
	}

	virtual FText GetVariantTitle() const override
	{
		return LOCTEXT("LinuxVariantTitle", "Build Type");
	}

	virtual float GetVariantPriority() const override
	{
		return TProperties::GetVariantPriority();
	}

	DECLARE_DERIVED_EVENT(TLinuxTargetPlatform, ITargetPlatform::FOnTargetDeviceDiscovered, FOnTargetDeviceDiscovered);
	virtual FOnTargetDeviceDiscovered& OnDeviceDiscovered( ) override
	{
		return DeviceDiscoveredEvent;
	}

	DECLARE_DERIVED_EVENT(TLinuxTargetPlatform, ITargetPlatform::FOnTargetDeviceLost, FOnTargetDeviceLost);
	virtual FOnTargetDeviceLost& OnDeviceLost( ) override
	{
		return DeviceLostEvent;
	}

	//~ End ITargetPlatform Interface

protected:

#if WITH_ENGINE
	/** Whether we're in process of changing device config - if yes, we will prevent recurrent calls. */
	bool bChangingDeviceConfig;

	void InitDevicesFromConfig()
	{
		if (bChangingDeviceConfig)
		{
			return;
		}
		bChangingDeviceConfig = true;

		int NumDevices = 0;
		for (;; ++NumDevices)
		{
			FString DeviceName, DeviceUser, DevicePass;

			FString DeviceBaseKey(FString::Printf(TEXT("LinuxTargetPlatfrom_%s_Device_%d"), *TSuper::PlatformName(), NumDevices));
			FString DeviceNameKey = DeviceBaseKey + TEXT("_Name");
			if (!GConfig->GetString(TEXT("/Script/LinuxTargetPlatform.LinuxTargetSettings"), *DeviceNameKey, DeviceName, GEngineIni))
			{
				// no such device
				break;
			}

			if (!AddDevice(DeviceName, false))
			{
				break;
			}

			// set credentials, if any
			FString DeviceUserKey = DeviceBaseKey + TEXT("_User");
			if (GConfig->GetString(TEXT("/Script/LinuxTargetPlatform.LinuxTargetSettings"), *DeviceUserKey, DeviceUser, GEngineIni))
			{
				FString DevicePassKey = DeviceBaseKey + TEXT("_Pass");
				if (GConfig->GetString(TEXT("/Script/LinuxTargetPlatform.LinuxTargetSettings"), *DevicePassKey, DevicePass, GEngineIni))
				{
					for (const auto & DeviceIter : Devices)
					{
						ITargetDevicePtr Device = DeviceIter.Value;
						if (Device.IsValid() && Device->GetId().GetDeviceName() == DeviceName)
						{
							Device->SetUserCredentials(DeviceUser, DevicePass);
						}
					}
				}
			}
		}

		bChangingDeviceConfig = false;
	}

	void SaveDevicesToConfig()
	{
		if (bChangingDeviceConfig)
		{
			return;
		}
		bChangingDeviceConfig = true;

		int DeviceIndex = 0;
		for (const auto & DeviceIter : Devices)
		{
			ITargetDevicePtr Device = DeviceIter.Value;

			FString DeviceBaseKey(FString::Printf(TEXT("LinuxTargetPlatfrom_%s_Device_%d"), *TSuper::PlatformName(), DeviceIndex));
			FString DeviceNameKey = DeviceBaseKey + TEXT("_Name");

			if (Device.IsValid())
			{
				FString DeviceName = Device->GetId().GetDeviceName();
				// do not save a local device on Linux or it will be duplicated
				if (PLATFORM_LINUX && DeviceName == FPlatformProcess::ComputerName())
				{
					continue;
				}

				GConfig->SetString(TEXT("/Script/LinuxTargetPlatform.LinuxTargetSettings"), *DeviceNameKey, *DeviceName, GEngineIni);

				FString DeviceUser, DevicePass;
				if (Device->GetUserCredentials(DeviceUser, DevicePass))
				{
					FString DeviceUserKey = DeviceBaseKey + TEXT("_User");
					FString DevicePassKey = DeviceBaseKey + TEXT("_Pass");

					GConfig->SetString(TEXT("/Script/LinuxTargetPlatform.LinuxTargetSettings"), *DeviceUserKey, *DeviceUser, GEngineIni);
					GConfig->SetString(TEXT("/Script/LinuxTargetPlatform.LinuxTargetSettings"), *DevicePassKey, *DevicePass, GEngineIni);
				}

				++DeviceIndex;	// needs to be incremented here since we cannot allow gaps
			}
		}

		bChangingDeviceConfig = false;
	}
#endif // WITH_ENGINE

	// Holds the local device.
	FLinuxTargetDevicePtr LocalDevice;
	// Holds a map of valid devices.
	TMap<FString, FLinuxTargetDevicePtr> Devices;


#if WITH_ENGINE
	// Holds the Engine INI settings for quick use.
	FConfigFile EngineSettings;

	// Holds the texture LOD settings.
	const UTextureLODSettings* TextureLODSettings;

	// Holds static mesh LOD settings.
	FStaticMeshLODSettings StaticMeshLODSettings;

	// True if the project requires encoded HDR reflection captures
	bool bRequiresEncodedHDRReflectionCaptures;
#endif // WITH_ENGINE

private:

	// Holds an event delegate that is executed when a new target device has been discovered.
	FOnTargetDeviceDiscovered DeviceDiscoveredEvent;

	// Holds an event delegate that is executed when a target device has been lost, i.e. disconnected or timed out.
	FOnTargetDeviceLost DeviceLostEvent;
};

#undef LOCTEXT_NAMESPACE
