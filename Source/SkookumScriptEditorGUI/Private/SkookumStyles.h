//=======================================================================================
// Copyright (c) 2001-2017 Agog Labs Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
//     http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//=======================================================================================

//=======================================================================================
// SkookumScript Plugin for Unreal Engine 4
//=======================================================================================

#pragma once

#include "UObject/NameTypes.h"
#include "Templates/SharedPointer.h"

//---------------------------------------------------------------------------------------

class FSkookumStyles
  {
  public:
    // Initializes the value of MenuStyleInstance and registers it with the Slate Style Registry.
    static void Initialize();

    // Unregisters the Slate Style Set and then resets the MenuStyleInstance pointer.
    static void Shutdown();

    // Retrieves a reference to the Slate Style pointed to by MenuStyleInstance.
    static const class ISlateStyle& Get();

    // Retrieves the name of the Style Set.
    static FName GetStyleSetName();

  private:

    // Singleton instance used for our Style Set.
    static TSharedPtr<class FSlateStyleSet> ms_singleton_p;

  };

