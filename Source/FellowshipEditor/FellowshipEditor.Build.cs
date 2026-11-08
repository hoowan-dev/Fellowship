// Copyright HoowanDev 2026 all rights reserved.

using UnrealBuildTool;

public class FellowshipEditor : ModuleRules
{
	public FellowshipEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"AdvancedPreviewScene",
			"AssetDefinition",
			"Core",
			"CoreUObject",
			"Engine",
			"Fellowship",
			"PropertyEditor",
			"Slate",
			"SlateCore",
			"UnrealEd"
		});
	}
}
