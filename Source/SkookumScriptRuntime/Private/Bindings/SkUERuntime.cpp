//=======================================================================================
// SkookumScript Plugin for Unreal Engine 4
// Copyright (c) 2015 Agog Labs Inc. All rights reserved.
//
// SkookumScript Runtime Hooks for Unreal - Input/Output Init/Update/Deinit Manager
// 
// Author: Conan Reis
//=======================================================================================


//=======================================================================================
// Includes
//=======================================================================================

#include "../SkookumScriptRuntimePrivatePCH.h"

#include "SkUERuntime.hpp"
#include "SkUERemote.hpp"
#include "SkUEBindings.hpp"
#include "SkUEClassBinding.hpp"

#include "GenericPlatformProcess.h"
#include <chrono>

#include <AgogCore/AMethodArg.hpp>
#include <SkookumScript/SkClass.hpp>

//=======================================================================================
// Local Global Structures
//=======================================================================================

namespace
{

  //---------------------------------------------------------------------------------------
  // Custom Unreal Binary Handle Structure
  struct SkBinaryHandleUE : public SkBinaryHandle
    {
    // Public Methods

      //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      SkBinaryHandleUE(void * binary_p, uint32_t size) : SkBinaryHandle(binary_p, size)
        {
        }

      //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      static SkBinaryHandleUE * create(const TCHAR * path_p)
        {
        FArchive * reader_p = IFileManager::Get().CreateFileReader(path_p);
        if (!reader_p)
          {
          return nullptr;
          }

        int64 size = reader_p->TotalSize();
        if (size == INDEX_NONE)
          {
          reader_p->Close();
          delete reader_p;

          return nullptr;
          }

        uint8 * binary_p = (uint8*)FMemory::Malloc(size);
        if (!binary_p)
          {
          reader_p->Close();
          delete reader_p;

          return nullptr;
          }

        reader_p->Serialize(binary_p, size);
        bool success = reader_p->Close();
        delete reader_p;

        if (!success)
          {
          delete binary_p;
          return nullptr;
          }

        return new SkBinaryHandleUE(binary_p, (uint32_t)size);
        }
    };


} // End unnamed namespace


//=======================================================================================
// SkUERuntime Methods
//=======================================================================================

//---------------------------------------------------------------------------------------

SkUERuntime::SkUERuntime() 
  : m_is_initialized(false)
  , m_is_compiled_scripts_loaded(false)
  , m_is_compiled_scripts_bound(false)
  , m_have_game_module(false)
  , m_compiled_file_b(false)
  , m_listener_manager(256, 256)
  , m_project_generated_bindings_p(nullptr)
  , m_editor_interface_p(nullptr)
  {
  ms_singleton_p = this;
  }

//---------------------------------------------------------------------------------------
// One-time initialization of SkookumScript
// See Also    shutdown()
// Author(s)   Conan Reis
void SkUERuntime::startup()
  {
  SK_ASSERTX(!m_is_initialized, "Tried to initialize SkUERuntime twice in a row.");

  A_DPRINT("\nSkookumScript starting up.\n");

  // Let scripting system know that the game engine is present and is being hooked-in
  SkDebug::enable_engine_present();

  #ifdef SKOOKUM_REMOTE_UNREAL
    SkDebug::register_print_with_agog();
  #endif

  SkBrain::ms_component_class_name = ASymbol::create("SkookumScriptBehaviorComponent");

  SkBrain::register_bind_atomics_func(SkRuntimeBase::bind_routines);
  SkClass::register_raw_resolve_func(SkUEClassBindingHelper::resolve_raw_data_static);
  SkookumScript::register_on_initialization_level_changed_func(SkRuntimeBase::initialization_level_changed);

  m_is_initialized = true;
  }

//---------------------------------------------------------------------------------------
// One-time shutdown of SkookumScript
void SkUERuntime::shutdown()
  {
  SK_ASSERTX(!SkookumScript::is_flag_set(SkookumScript::Flag_updating), "Attempting to shut down SkookumScript while it is in the middle of an update.");
  SK_ASSERTX(m_is_initialized, "Tried to shut down SkUERuntime without prior initialization.");

  // Printing during shutdown will re-launch IDE in case it has been closed prior to UE4
  // So quick fix is to just not print during shutdown
  //A_DPRINT("\nSkookumScript shutting down.\n");

  #ifdef SKOOKUM_REMOTE_UNREAL
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // Disconnect from remote client
    SkRemoteBase::ms_default_p->set_mode(SkLocale_embedded);
  #endif

  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // Clears out Blueprint interface mappings
  SkUEBlueprintInterface::get()->clear();

  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // Unloads SkookumScript and cleans-up
  if (SkookumScript::get_initialization_level() > SkookumScript::InitializationLevel_none)
    {
    SkookumScript::deinitialize_sim();
    SkookumScript::deinitialize_program();
    SkookumScript::deinitialize();
    }

  // Keep track just in case
  m_is_compiled_scripts_loaded = false;
  m_is_compiled_scripts_bound = false;

  // Gets rid of registered bind functions
  SkBrain::unregister_all_bind_atomics_funcs();

  // Get rid of symbol so references are released
  SkBrain::ms_component_class_name = ASymbol::get_null();

  // Deinitialize custom UE4 classes
  USkookumScriptBehaviorComponent::deinitialize();

  m_is_initialized = false;
  }

//---------------------------------------------------------------------------------------

void SkUERuntime::ensure_static_ue_types_registered()
  {
  SkUEBindings::ensure_static_ue_types_registered(m_project_generated_bindings_p);
  }

//---------------------------------------------------------------------------------------
// Override to add bindings to any custom C++ routines (methods & coroutines).
//
// #See Also   SkBrain::register_bind_atomics_func()
// #Modifiers  virtual
// #Author(s)  Conan Reis
void SkUERuntime::on_bind_routines()
  {
  A_DPRINT(A_SOURCE_STR "\nBind routines for SkUERuntime.\n");

  #if WITH_EDITORONLY_DATA
    SkUEClassBindingHelper::reset_dynamic_class_mappings(); // Start over fresh
  #endif

  ensure_static_ue_types_registered();
  SkUEBindings::finish_register_bindings(m_project_generated_bindings_p);
  USkookumScriptBehaviorComponent::initialize();
  
  // We bound all routines at least once
  m_is_compiled_scripts_bound = true;
  }

//---------------------------------------------------------------------------------------

void SkUERuntime::on_initialization_level_changed(SkookumScript::eInitializationLevel from_level, SkookumScript::eInitializationLevel to_level)
  {
  // Invalidate all invokable pointers if a program was just deleted
  if (from_level == SkookumScript::InitializationLevel_program && to_level < from_level)
    {
    m_blueprint_interface.clear_all_sk_invokables();
    }
  
  // Re-resolve invokable pointers if a program has just been loaded
  if (to_level == SkookumScript::InitializationLevel_program && from_level < to_level)
    {
    m_blueprint_interface.rebind_all_sk_invokables();
    }
  }

//---------------------------------------------------------------------------------------

void SkUERuntime::set_project_generated_bindings(SkUEBindingsInterface * project_generated_bindings_p)
  {
  SK_ASSERTX(!m_is_compiled_scripts_bound || !project_generated_bindings_p, "Tried to set project bindings but routines have already been bound to the default generated bindings. You need to call this function earlier in the initialization sequence.");
  m_project_generated_bindings_p = project_generated_bindings_p;

  if (project_generated_bindings_p)
    {
    m_have_game_module = true;

    // Now that bindings are known, bind the atomics
    bind_compiled_scripts();
    }
  }

//---------------------------------------------------------------------------------------

void SkUERuntime::reexpose_all_to_blueprints(bool is_final)
  {
  #if WITH_EDITOR
    AMethodArg<ISkookumScriptRuntimeEditorInterface, UClass*> editor_on_class_updated_f(m_editor_interface_p, &ISkookumScriptRuntimeEditorInterface::on_class_updated);
    tSkUEOnClassUpdatedFunc * on_class_updated_f = &editor_on_class_updated_f;
  #else
    tSkUEOnClassUpdatedFunc * on_class_updated_f = nullptr;
  #endif
  m_blueprint_interface.reexpose_all(on_class_updated_f, is_final); // Hook up Blueprint functions and events for static classes
  }

//---------------------------------------------------------------------------------------
// Determine the compiled file path
//   - usually Content\SkookumScript\Compiled[bits]\Classes.sk-bin
// 
// #Author(s):  Conan Reis
const FString & SkUERuntime::get_compiled_path() const
  {
  if (!m_compiled_file_b)
    {
    m_compiled_file_b = content_file_exists(TEXT("classes.sk-bin"), &m_compiled_path);
    }

  return m_compiled_path;
  }

//---------------------------------------------------------------------------------------
// Determine if a given file name exists in the content/SkookumScript folder of either game or engine
// #Author(s): Markus Breyer
bool SkUERuntime::content_file_exists(const TCHAR * file_name_p, FString * folder_path_p) const
  {
  FString folder_path;

  // Check if we got a game
  FString game_name(FApp::GetGameName());
  if (game_name.IsEmpty())
    {
    // No game, look for default project binaries
    folder_path = FPaths::EngineContentDir() / TEXT("SkookumScript") /*TEXT(SK_BITS_ID)*/;
    if (!FPaths::FileExists(folder_path / file_name_p))
      {
      return false;
      }
    }
  else
    {
    // We have a game, so first check if it exists in the game content folder
    folder_path = FPaths::GameContentDir() / TEXT("SkookumScript") /*TEXT(SK_BITS_ID)*/;
    if (!FPaths::FileExists(folder_path / file_name_p))
      {
      #if WITH_EDITORONLY_DATA
        // If not found in game, check in temp location
        folder_path = FPaths::GameIntermediateDir() / TEXT("SkookumScript/Content/SkookumScript");
        if (!FPaths::FileExists(folder_path / file_name_p))
          {
          return false;
          }
      #else
        return false;
      #endif
      }
    }

  *folder_path_p = folder_path;
  return true;
  }

//---------------------------------------------------------------------------------------
// Load the Skookum class hierarchy scripts in compiled binary form.
// 
// #Returns
//   true if compiled scrips successfully loaded, false if not
// 
// #See:        load_compiled_scripts()
// #Modifiers:  static
// #Author(s):  Conan Reis
bool SkUERuntime::load_compiled_scripts()
  {
  SK_ASSERTX(m_is_initialized, "SkookumScruipt must be initialized to be able to load compiled scripts.");

  A_DPRINT("\nSkookumScript loading previously parsed compiled binary...\n");

  if (load_compiled_hierarchy() != SkLoadStatus_ok)
    {
    return false;
    }

  A_DPRINT("  ...done!\n\n");

  // After fresh loading of binaries, there are no bindings
  m_is_compiled_scripts_loaded = true;
  m_is_compiled_scripts_bound = false;

  // After loading, hook up a few things right away
  ensure_static_ue_types_registered();
  SkUEBindings::begin_register_bindings();
  // In commandlet mode, expose here, even if types cannot be fully resolved, to avoid compile errors
  if (IsRunningCommandlet())
    {
    reexpose_all_to_blueprints(false);
    }

  return true;
  }

//---------------------------------------------------------------------------------------
// Bind atomic routines to the compiled binary loaded by load_compiled_scripts()
// 
// #Params
//   ensure_atomics:
//     If set makes sure all atomic (C++) scripts that were expecting a C++ function to be
//     bound/hooked up to them do actually have a binding.
//   ignore_classes_pp:
//     array of classes to ignore when ensure_atomics is set.
//     This allows some classes with optional or delayed bindings to be skipped such as
//     bindings to a in-game world editor.
//   ignore_count:  number of class pointers in ignore_classes_pp
// 
// #See:        load_compiled_class_group()
// #Author(s):  Markus Breyer
void SkUERuntime::bind_compiled_scripts(
  bool       ensure_atomics,     // = true
  SkClass ** ignore_classes_pp,  // = nullptr
  uint32_t   ignore_count        // = 0u
  )
  {
  SK_ASSERTX(m_is_initialized, "SkookumScript must be initialized to be able to bind compiled scripts.");
  SK_ASSERTX(m_is_compiled_scripts_loaded, "Compiled binaries must be loaded to be able to bind.");

  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // Bind atomics
  A_DPRINT("SkookumScript binding with C++ routines...\n");

  // Registers/connects Generic SkookumScript atomic classes, stimuli, coroutines, etc.
  // with the compiled binary that was just loaded.
  SkookumScript::initialize_program();

  #if (SKOOKUM & SK_DEBUG)
    // Ensure atomic (C++) methods/coroutines are properly bound to their C++ equivalents
    if (ensure_atomics)
      {
      SkBrain::ensure_atomics_registered(ignore_classes_pp, ignore_count);
      }
  #endif

  A_DPRINT("  ...done!\n\n");

  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // Enable SkookumScript evaluation
  A_DPRINT("SkookumScript initializing session...\n");
  SkookumScript::initialize_sim();
  A_DPRINT("  ...done!\n\n");
  }

//---------------------------------------------------------------------------------------
// Load Skookum class hierarchy scripts in compiled binary form and bind atomic routines
//
// #See:        load_compiled_scripts(), bind_compiled_scripts()
// #Author(s):  Markus Breyer
bool SkUERuntime::load_and_bind_compiled_scripts(
  bool       ensure_atomics,     // = true
  SkClass ** ignore_classes_pp,  // = nullptr
  uint32_t   ignore_count        // = 0u
  )
  {
  bool success = load_compiled_scripts();
  if (success)
    {
    bind_compiled_scripts(ensure_atomics, ignore_classes_pp, ignore_count);
    }
  return success;
  }

//---------------------------------------------------------------------------------------
// Determines if binary for class hierarchy and associated info exists.
// 
// This check is done before the symbol file check since the symbol file is only needed
// when debugging and giving an error about missing the compiled binary is more intuitive
// to the end user than a missing symbol file.
// 
// #See Also:   get_binary_hierarchy()
// #Modifiers:  virtual - overridden from SkRuntimeBase
// #Author(s):  Conan Reis
bool SkUERuntime::is_binary_hierarchy_existing()
  {
  get_compiled_path();

  return m_compiled_file_b;
  }

//---------------------------------------------------------------------------------------
// Get notified that the compiled binaries moved to a new location
void SkUERuntime::on_binary_hierarchy_path_changed()
  {
  m_compiled_file_b = false;
  }

//---------------------------------------------------------------------------------------
// Gets memory representing binary for class hierarchy and associated info.
// 
// #See Also:   load_compiled_scripts()
// #Modifiers:  virtual - overridden from SkRuntimeBase
// #Author(s):  Conan Reis
SkBinaryHandle * SkUERuntime::get_binary_hierarchy()
  {
  FString compiled_file = FPaths::ConvertRelativePathToFull(get_compiled_path() / TEXT("classes.sk-bin"));

  A_DPRINT("  Loading compiled binary file '%ls'...\n", *compiled_file);

  return SkBinaryHandleUE::create(*compiled_file);
  }

//---------------------------------------------------------------------------------------
// Gets memory representing binary for group of classes with specified class as root.
// Used as a mechanism to "demand load" scripts.
// 
// #See Also:   load_compiled_scripts()
// #Modifiers:  virtual - overridden from SkRuntimeBase
// #Author(s):  Conan Reis
SkBinaryHandle * SkUERuntime::get_binary_class_group(const SkClass & cls)
  {
  FString compiled_file = get_compiled_path();
  
  // $Revisit - CReis Should use fast custom uint32_t to hex string function.
  compiled_file += a_cstr_format("/Class[%x].sk-bin", cls.get_name_id());
  return SkBinaryHandleUE::create(*compiled_file);
  }


#if defined(A_SYMBOL_STR_DB_AGOG)  

//---------------------------------------------------------------------------------------
// Gets memory representing binary for class hierarchy and associated info.
// 
// #See Also:   load_compiled_scripts()
// #Modifiers:  virtual - overridden from SkRuntimeBase
// #Author(s):  Conan Reis
SkBinaryHandle * SkUERuntime::get_binary_symbol_table()
  {
  FString sym_file = FPaths::ConvertRelativePathToFull(get_compiled_path() / TEXT("classes.sk-sym"));

  A_DPRINT("  Loading compiled binary symbol file '%ls'...\n", *sym_file);

  SkBinaryHandleUE * handle_p = SkBinaryHandleUE::create(*sym_file);

  // Ensure symbol table binary exists
  if (!handle_p)
    {
    A_DPRINT("  ...it does not exist!\n\n", *sym_file);
    }

  return handle_p;
  }                                                                  

#endif


//---------------------------------------------------------------------------------------
// #Author(s):  Conan Reis
void SkUERuntime::release_binary(SkBinaryHandle * handle_p)
  {
  delete static_cast<SkBinaryHandleUE *>(handle_p);
  }



