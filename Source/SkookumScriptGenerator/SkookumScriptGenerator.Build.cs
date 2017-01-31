// Copyright (c) 2014-2016 Agog Labs, Inc. All Rights Reserved.
using System.IO;
using UnrealBuildTool;

namespace UnrealBuildTool.Rules
{
  public class SkookumScriptGenerator : ModuleRules
  {
    public SkookumScriptGenerator(TargetInfo Target)
    {			
      // We need ICU for regex use!
      UEBuildConfiguration.bCompileICU = true;

      PublicIncludePaths.AddRange(
        new string[] {					
          "Programs/UnrealHeaderTool/Public",
          // ... add other public include paths required here ...
        }
        );

      PrivateIncludePaths.AddRange(
        new string[] {
          // "Developer/SkookumScriptGenerator/Private",
          // ... add other private include paths required here ...
        }
        );

      PublicDependencyModuleNames.AddRange(
        new string[]
        {
          "Core",
          "CoreUObject",
          // ... add other public dependencies that you statically link with here ...
        }
        );

      PrivateDependencyModuleNames.AddRange(
        new string[]
        {
          // ... add private dependencies that you statically link with here ...
        }
        );

      DynamicallyLoadedModuleNames.AddRange(
        new string[]
        {
          // ... add any modules that your module loads dynamically here ...
        }
        );
    }
  }
}