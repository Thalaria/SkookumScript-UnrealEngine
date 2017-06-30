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

#include "ISkookumScriptEditor.h"
#include "CoreUObject.h"
#include "ModuleManager.h"
#include "Engine.h"
#include "UnrealEd.h"
#include "SlateBasics.h"
#include "SlateGameResources.h" 
#include "IPluginManager.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/StructureEditorUtils.h"
#include "Kismet2/EnumEditorUtils.h"
#include "ISkookumScriptRuntime.h"
#include "AssetRegistryModule.h"
#include "BlueprintActionDatabase.h"
#include "BlueprintEditorUtils.h"
#include "Engine/UserDefinedEnum.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"

#include "GraphEditor.h"

#include "../../SkookumScriptGenerator/Private/SkookumScriptGeneratorBase.inl"

DEFINE_LOG_CATEGORY_STATIC(LogSkookumScriptEditor, Log, All);

//---------------------------------------------------------------------------------------

class FSkookumScriptEditor 
  : public ISkookumScriptEditor
  , public ISkookumScriptRuntimeEditorInterface
  , public FStructureEditorUtils::FStructEditorManager::ListenerType
  , public FEnumEditorUtils::FEnumEditorManager::ListenerType
{
public:

protected:

  //---------------------------------------------------------------------------------------
  // IModuleInterface implementation

  virtual void  StartupModule() override;
  virtual void  ShutdownModule() override;

  //---------------------------------------------------------------------------------------
  // ISkookumScriptRuntimeEditorInterface implementation

  virtual void  on_class_updated(UClass * ue_class_p) override;
  virtual void  on_function_updated(UFunction * ue_function_p, bool is_event) override;
  virtual void  on_function_removed_from_class(UClass * ue_class_p) override;
  virtual bool  check_out_file(const FString & file_path) const override;

  //---------------------------------------------------------------------------------------
  // FStructureEditorManager::ListenerType implementation

  virtual void PreChange(const UUserDefinedStruct * struct_p, FStructureEditorUtils::EStructureEditorChangeInfo change_type) override;
  virtual void PostChange(const UUserDefinedStruct * struct_p, FStructureEditorUtils::EStructureEditorChangeInfo change_type) override;

  //---------------------------------------------------------------------------------------
  // FEnumEditorManager::ListenerType implementation

  virtual void PreChange(const UUserDefinedEnum * enum_p, FEnumEditorUtils::EEnumEditorChangeInfo change_type) override;
  virtual void PostChange(const UUserDefinedEnum * enum_p, FEnumEditorUtils::EEnumEditorChangeInfo change_type) override;

  //---------------------------------------------------------------------------------------
  // Local implementation

  ISkookumScriptRuntime * get_runtime() const { return static_cast<ISkookumScriptRuntime *>(m_runtime_p.Get()); }

  void                    on_asset_loaded(UObject * obj_p);
  void                    on_object_modified(UObject * obj_p);
  void                    on_new_asset_created(UFactory * factory_p);
  void                    on_assets_deleted(const TArray<UClass*> & deleted_asset_classes);
  void                    on_asset_post_import(UFactory * factory_p, UObject * obj_p);
  void                    on_asset_added(const FAssetData & asset_data);
  void                    on_asset_renamed(const FAssetData & asset_data, const FString & old_object_path);
  void                    on_in_memory_asset_created(UObject * obj_p);
  void                    on_in_memory_asset_deleted(UObject * obj_p);
  void                    on_map_opened(const FString & file_name, bool as_template);

  void                    on_new_asset(UObject * obj_p);

  // Data members

  TSharedPtr<IModuleInterface>  m_runtime_p;  // TSharedPtr holds on to the module so it can't go away while we need it

  FDelegateHandle               m_on_asset_loaded_handle;
  FDelegateHandle               m_on_object_modified_handle;
  FDelegateHandle               m_on_map_opened_handle;
  FDelegateHandle               m_on_new_asset_created_handle;
  FDelegateHandle               m_on_assets_deleted_handle;
  FDelegateHandle               m_on_asset_post_import_handle;
  FDelegateHandle               m_on_asset_added_handle;
  FDelegateHandle               m_on_asset_renamed_handle;
  FDelegateHandle               m_on_in_memory_asset_created_handle;
  FDelegateHandle               m_on_in_memory_asset_deleted_handle;

  };

IMPLEMENT_MODULE(FSkookumScriptEditor, SkookumScriptEditor)

//=======================================================================================
// IModuleInterface implementation
//=======================================================================================

//---------------------------------------------------------------------------------------

void FSkookumScriptEditor::StartupModule()
  {
  // Get pointer to runtime module
  m_runtime_p = FModuleManager::Get().GetModule("SkookumScriptRuntime");

  // Don't do anything if SkookumScript is not active
  if (get_runtime()->is_skookum_disabled())
    {
    return;
    }

  // Tell runtime that editor is present (needed even in commandlet mode as we might have to demand-load blueprints)
  get_runtime()->set_editor_interface(this);

  if (!IsRunningCommandlet())
    {
    // Hook up delegates
    m_on_asset_loaded_handle          = FCoreUObjectDelegates::OnAssetLoaded.AddRaw(this, &FSkookumScriptEditor::on_asset_loaded);
    m_on_object_modified_handle       = FCoreUObjectDelegates::OnObjectModified.AddRaw(this, &FSkookumScriptEditor::on_object_modified);
    m_on_map_opened_handle            = FEditorDelegates::OnMapOpened.AddRaw(this, &FSkookumScriptEditor::on_map_opened);
    m_on_new_asset_created_handle     = FEditorDelegates::OnNewAssetCreated.AddRaw(this, &FSkookumScriptEditor::on_new_asset_created);
    m_on_assets_deleted_handle        = FEditorDelegates::OnAssetsDeleted.AddRaw(this, &FSkookumScriptEditor::on_assets_deleted);
    m_on_asset_post_import_handle     = FEditorDelegates::OnAssetPostImport.AddRaw(this, &FSkookumScriptEditor::on_asset_post_import);

    FAssetRegistryModule & asset_registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryConstants::ModuleName);
    m_on_asset_added_handle             = asset_registry.Get().OnAssetAdded().AddRaw(this, &FSkookumScriptEditor::on_asset_added);
    m_on_asset_renamed_handle           = asset_registry.Get().OnAssetRenamed().AddRaw(this, &FSkookumScriptEditor::on_asset_renamed);
    m_on_in_memory_asset_created_handle = asset_registry.Get().OnInMemoryAssetCreated().AddRaw(this, &FSkookumScriptEditor::on_in_memory_asset_created);
    m_on_in_memory_asset_deleted_handle = asset_registry.Get().OnInMemoryAssetDeleted().AddRaw(this, &FSkookumScriptEditor::on_in_memory_asset_deleted);

    // Instrument all already existing blueprints
    TArray<UObject*> blueprint_array;
    GetObjectsOfClass(UBlueprint::StaticClass(), blueprint_array);
    for (UObject * obj_p : blueprint_array)
      {
      on_new_asset(obj_p);
      }

    // Same for user defined enums
    TArray<UObject*> enum_array;
    GetObjectsOfClass(UUserDefinedEnum::StaticClass(), enum_array);
    for (UObject * obj_p : enum_array)
      {
      on_new_asset(obj_p);
      }
    }
  }

//---------------------------------------------------------------------------------------

void FSkookumScriptEditor::ShutdownModule()
  {
  // Don't do anything if SkookumScript is not active
  if (get_runtime()->is_skookum_disabled())
    {
    return;
    }

  get_runtime()->set_editor_interface(nullptr);
  m_runtime_p.Reset();

  if (!IsRunningCommandlet())
    {
    // Remove delegates
    FCoreUObjectDelegates::OnAssetLoaded.Remove(m_on_asset_loaded_handle);
    FCoreUObjectDelegates::OnObjectModified.Remove(m_on_object_modified_handle);
    FEditorDelegates::OnMapOpened.Remove(m_on_map_opened_handle);
    FEditorDelegates::OnNewAssetCreated.Remove(m_on_new_asset_created_handle);
    FEditorDelegates::OnAssetsDeleted.Remove(m_on_assets_deleted_handle);
    FEditorDelegates::OnAssetPostImport.Remove(m_on_asset_post_import_handle);

    FAssetRegistryModule * asset_registry_p = FModuleManager::GetModulePtr<FAssetRegistryModule>(AssetRegistryConstants::ModuleName);
    if (asset_registry_p)
      {
      asset_registry_p->Get().OnAssetAdded().Remove(m_on_asset_added_handle);
      asset_registry_p->Get().OnAssetRenamed().Remove(m_on_asset_renamed_handle);
      asset_registry_p->Get().OnInMemoryAssetCreated().Remove(m_on_in_memory_asset_created_handle);
      asset_registry_p->Get().OnInMemoryAssetDeleted().Remove(m_on_in_memory_asset_deleted_handle);
      }
    }
  }

//=======================================================================================
// ISkookumScriptRuntimeEditorInterface implementation
//=======================================================================================

//---------------------------------------------------------------------------------------

void FSkookumScriptEditor::on_class_updated(UClass * ue_class_p)
  {
  // Nothing to do if no engine
  if (!GEngine) return;

  // 1) Refresh actions (in Blueprint editor drop down menu)
  FBlueprintActionDatabase::Get().RefreshClassActions(ue_class_p);

  // Remember affected Blueprints here
  TArray<UBlueprint *> affected_blueprints;

  // Storage for gathered objects
  TArray<UObject*> obj_array;

  // 2) Refresh node display of all SkookumScript function call nodes
  obj_array.Reset();
  GetObjectsOfClass(UK2Node_CallFunction::StaticClass(), obj_array, true, RF_ClassDefaultObject);
  for (auto obj_p : obj_array)
    {
    UK2Node_CallFunction * function_node_p = Cast<UK2Node_CallFunction>(obj_p);
    UFunction * target_function_p = function_node_p->GetTargetFunction();
    // Also refresh all nodes with no target function as it is probably a Sk function that was deleted
    //if (!target_function_p || get_runtime()->is_skookum_reflected_call(target_function_p))
    if (target_function_p 
     && get_runtime()->is_skookum_reflected_call(target_function_p)
     && ue_class_p->IsChildOf(target_function_p->GetOwnerClass()))
      {
      function_node_p->ReconstructNode();
      affected_blueprints.AddUnique(FBlueprintEditorUtils::FindBlueprintForNode(function_node_p));
      }
    }

  // 3) Refresh node display of all SkookumScript event nodes
  obj_array.Reset();
  GetObjectsOfClass(UK2Node_Event::StaticClass(), obj_array, true, RF_ClassDefaultObject);
  for (auto obj_p : obj_array)
    {
    UK2Node_Event * event_node_p = Cast<UK2Node_Event>(obj_p);
    UFunction * event_function_p = event_node_p->FindEventSignatureFunction();
    if (event_function_p 
     && get_runtime()->is_skookum_reflected_event(event_function_p)
     && ue_class_p->IsChildOf(event_function_p->GetOwnerClass()))
      {
      event_node_p->ReconstructNode();
      affected_blueprints.AddUnique(FBlueprintEditorUtils::FindBlueprintForNode(event_node_p));
      }
    }

  // 4) Try recompiling any affected Blueprint that previously had errors
  for (UBlueprint * blueprint_p : affected_blueprints)
    {
    if (blueprint_p && blueprint_p->Status == BS_Error)
      {
      FKismetEditorUtilities::CompileBlueprint(blueprint_p);
      }
    }
  }

//---------------------------------------------------------------------------------------

void FSkookumScriptEditor::on_function_updated(UFunction * ue_function_p, bool is_event)
  {
  // Nothing to do if no engine
  if (!GEngine) return;

  // 1) Refresh actions (in Blueprint editor drop down menu)
  FBlueprintActionDatabase::Get().RefreshClassActions(ue_function_p->GetOwnerClass());

  // Remember affected Blueprints here
  TArray<UBlueprint *> affected_blueprints;

  // 2) Refresh node display of all nodes using this function
  if (is_event)
    {
    TArray<UObject*> obj_array;
    GetObjectsOfClass(UK2Node_Event::StaticClass(), obj_array, true, RF_ClassDefaultObject);
    for (auto obj_p : obj_array)
      {
      UK2Node_Event * event_node_p = Cast<UK2Node_Event>(obj_p);
      // Since Blueprint exposed function names are fully qualified, a plain name check should suffice
      if (event_node_p->EventReference.GetMemberName() == ue_function_p->GetFName())
        {
        event_node_p->ReconstructNode();
        affected_blueprints.AddUnique(FBlueprintEditorUtils::FindBlueprintForNode(event_node_p));
        }
      }
    }
  else
    {
    TArray<UObject*> obj_array;
    GetObjectsOfClass(UK2Node_CallFunction::StaticClass(), obj_array, true, RF_ClassDefaultObject);
    for (auto obj_p : obj_array)
      {
      UK2Node_CallFunction * function_node_p = Cast<UK2Node_CallFunction>(obj_p);
      // Since Blueprint exposed function names are fully qualified, a plain name check should suffice
      if (function_node_p->FunctionReference.GetMemberName() == ue_function_p->GetFName())
        {
        function_node_p->ReconstructNode();
        affected_blueprints.AddUnique(FBlueprintEditorUtils::FindBlueprintForNode(function_node_p));
        }
      }
    }

  // 3) Try recompiling any affected Blueprint that previously had errors
  for (UBlueprint * blueprint_p : affected_blueprints)
    {
    if (blueprint_p && blueprint_p->Status == BS_Error)
      {
      FKismetEditorUtilities::CompileBlueprint(blueprint_p);
      }
    }
  }

//---------------------------------------------------------------------------------------

void FSkookumScriptEditor::on_function_removed_from_class(UClass * ue_class_p)
  {
  // Refresh actions (in Blueprint editor drop down menu)
  FBlueprintActionDatabase::Get().RefreshClassActions(ue_class_p);
  }

//---------------------------------------------------------------------------------------

bool FSkookumScriptEditor::check_out_file(const FString & file_path) const
  {
  if (!ISourceControlModule::Get().IsEnabled())
    {
    return false;
    }

  return SourceControlHelpers::CheckOutFile(file_path);
  }

//=======================================================================================
// FStructureEditorManager::ListenerType implementation
//=======================================================================================

//---------------------------------------------------------------------------------------

void FSkookumScriptEditor::PreChange(const UUserDefinedStruct * struct_p, FStructureEditorUtils::EStructureEditorChangeInfo change_type)
  {
  }

//---------------------------------------------------------------------------------------

void FSkookumScriptEditor::PostChange(const UUserDefinedStruct * struct_p, FStructureEditorUtils::EStructureEditorChangeInfo change_type)
  {
  on_object_modified((UObject *)struct_p);
  }

//=======================================================================================
// FEnumEditorManager::ListenerType implementation
//=======================================================================================

//---------------------------------------------------------------------------------------

void FSkookumScriptEditor::PreChange(const UUserDefinedEnum * enum_p, FEnumEditorUtils::EEnumEditorChangeInfo change_type)
  {
  }

//---------------------------------------------------------------------------------------

void FSkookumScriptEditor::PostChange(const UUserDefinedEnum * enum_p, FEnumEditorUtils::EEnumEditorChangeInfo change_type)
  {
  on_object_modified((UObject *)enum_p);
  }

//=======================================================================================
// FSkookumScriptEditor implementation
//=======================================================================================

//---------------------------------------------------------------------------------------

void FSkookumScriptEditor::on_asset_loaded(UObject * obj_p)
  {
  on_new_asset(obj_p);
  }

//---------------------------------------------------------------------------------------
//
void FSkookumScriptEditor::on_object_modified(UObject * obj_p)
  {
  // Is this a blueprint?
  UBlueprint * blueprint_p = Cast<UBlueprint>(obj_p);
  if (blueprint_p)
    {    
    get_runtime()->on_class_added_or_modified(blueprint_p, false);
    }

  // Is this a struct?
  UUserDefinedStruct * struct_p = Cast<UUserDefinedStruct>(obj_p);
  if (struct_p)
    {
    get_runtime()->on_struct_added_or_modified(struct_p, false);
    }

  // Is this an enum?
  UUserDefinedEnum * enum_p = Cast<UUserDefinedEnum>(obj_p);
  if (enum_p)
    {
    get_runtime()->on_enum_added_or_modified(enum_p, false);
    }
  }

//---------------------------------------------------------------------------------------
//
void FSkookumScriptEditor::on_new_asset_created(UFactory * factory_p)
  {
  }

//---------------------------------------------------------------------------------------
//
void FSkookumScriptEditor::on_assets_deleted(const TArray<UClass*> & deleted_asset_classes)
  {
  }

//---------------------------------------------------------------------------------------
//
void FSkookumScriptEditor::on_asset_post_import(UFactory * factory_p, UObject * obj_p)
  {
  on_object_modified(obj_p);
  }

//---------------------------------------------------------------------------------------
// An asset is being added to the asset registry - during load, or otherwise
void FSkookumScriptEditor::on_asset_added(const FAssetData & asset_data)
  {
  }

//---------------------------------------------------------------------------------------
//
void FSkookumScriptEditor::on_asset_renamed(const FAssetData & asset_data, const FString & old_object_path)
  {
  static FName s_blueprint_class_name(TEXT("Blueprint"));
  static FName s_struct_class_name(TEXT("UserDefinedStruct"));
  static FName s_enum_class_name(TEXT("UserDefinedEnum"));

  if (asset_data.AssetClass == s_blueprint_class_name)
    {
    UBlueprint * blueprint_p = FindObjectChecked<UBlueprint>(ANY_PACKAGE, *asset_data.AssetName.ToString());
    if (blueprint_p)
      {
      get_runtime()->on_class_renamed(blueprint_p, FPaths::GetBaseFilename(old_object_path));
      }
    }
  else if (asset_data.AssetClass == s_struct_class_name)
    {
    UUserDefinedStruct * struct_p = FindObjectChecked<UUserDefinedStruct>(ANY_PACKAGE, *asset_data.AssetName.ToString());
    if (struct_p)
      {
      get_runtime()->on_struct_renamed(struct_p, FPaths::GetBaseFilename(old_object_path));
      }
    }
  else if (asset_data.AssetClass == s_enum_class_name)
    {
    UUserDefinedEnum * enum_p = FindObjectChecked<UUserDefinedEnum>(ANY_PACKAGE, *asset_data.AssetName.ToString());
    if (enum_p)
      {
      get_runtime()->on_enum_renamed(enum_p, FPaths::GetBaseFilename(old_object_path));
      }
    }
  }

//---------------------------------------------------------------------------------------
//
void FSkookumScriptEditor::on_in_memory_asset_created(UObject * obj_p)
  {
  on_new_asset(obj_p);
  }

//---------------------------------------------------------------------------------------
//
void FSkookumScriptEditor::on_in_memory_asset_deleted(UObject * obj_p)
  {
  UBlueprint * blueprint_p = Cast<UBlueprint>(obj_p);
  if (blueprint_p)
    {
    get_runtime()->on_class_deleted(blueprint_p);
    }

  UUserDefinedStruct * struct_p = Cast<UUserDefinedStruct>(obj_p);
  if (struct_p)
    {
    get_runtime()->on_struct_deleted(struct_p);
    }

  UUserDefinedEnum * enum_p = Cast<UUserDefinedEnum>(obj_p);
  if (enum_p)
    {
    get_runtime()->on_enum_deleted(enum_p);
    }
  }

//---------------------------------------------------------------------------------------
// Called when the map is done loading (load progress reaches 100%)
void FSkookumScriptEditor::on_map_opened(const FString & file_name, bool as_template)
  {
  // Let runtime know we are done opening a new map
  get_runtime()->on_editor_map_opened();
  }

//---------------------------------------------------------------------------------------

void FSkookumScriptEditor::on_new_asset(UObject * obj_p)
  {
  UBlueprint * blueprint_p = Cast<UBlueprint>(obj_p);
  if (blueprint_p)
    {
    get_runtime()->on_class_added_or_modified(blueprint_p, true);
    }

  UUserDefinedStruct * struct_p = Cast<UUserDefinedStruct>(obj_p);
  if (struct_p)
    {
    get_runtime()->on_struct_added_or_modified(struct_p, true);
    }

  UUserDefinedEnum * enum_p = Cast<UUserDefinedEnum>(obj_p);
  if (enum_p)
    {
    get_runtime()->on_enum_added_or_modified(enum_p, true);
    }
  }

