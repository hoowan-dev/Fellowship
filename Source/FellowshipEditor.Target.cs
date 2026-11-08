// Copyright HoowanDev 2026 all rights reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class FellowshipEditorTarget : TargetRules
{
	public FellowshipEditorTarget( TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
		ExtraModuleNames.Add("Fellowship");
		ExtraModuleNames.Add("FellowshipEditor");
	}
}
