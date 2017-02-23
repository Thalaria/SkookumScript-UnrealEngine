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

#include "SkookumScriptEditorGUIPrivatePCH.h"

#include "SkookumScriptEditorCommands.h"

PRAGMA_DISABLE_OPTIMIZATION
void FSkookumScriptEditorCommands::RegisterCommands()
  {
  #define LOCTEXT_NAMESPACE "SkookumScript"

    UI_COMMAND(m_skookum_button, "SkookumIDE", "Show in SkookumIDE", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Alt, EKeys::Tilde));

  #undef LOCTEXT_NAMESPACE
  }
PRAGMA_ENABLE_OPTIMIZATION
