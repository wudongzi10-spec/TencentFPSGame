// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TencentFPSDemo : ModuleRules
{
	public TencentFPSDemo(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"SlateCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"TencentFPSDemo",
			"TencentFPSDemo/Variant_Horror",
			"TencentFPSDemo/Variant_Horror/UI",
			"TencentFPSDemo/Variant_Shooter",
			"TencentFPSDemo/Variant_Shooter/AI",
			"TencentFPSDemo/Variant_Shooter/UI",
			"TencentFPSDemo/Variant_Shooter/Weapons"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
