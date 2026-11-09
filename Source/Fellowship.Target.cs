// Copyright HoowanDev 2026 all rights reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class FellowshipTarget : TargetRules
{
	public FellowshipTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V8;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_9;
		ExtraModuleNames.Add("Fellowship");
	}
}
