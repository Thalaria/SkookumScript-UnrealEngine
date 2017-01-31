//=======================================================================================
// SkookumScript Plugin for Unreal Engine 4
// Copyright (c) 2015 Agog Labs Inc. All rights reserved.
//
// Class for interfacing with UE4 Blueprint graphs 
//
// Author: Markus Breyer
//=======================================================================================

#include "../SkookumScriptRuntimePrivatePCH.h"

#include "SkUEBlueprintInterface.hpp"
#include "VectorMath/SkVector3.hpp"
#include "VectorMath/SkRotationAngles.hpp"
#include "VectorMath/SkTransform.hpp"
#include "Engine/SkUEEntity.hpp"
#include "Engine/SkUEActor.hpp"
#include "SkUEUtils.hpp"

#include "UObject/Package.h"

#include <SkookumScript/SkExpressionBase.hpp>
#include <SkookumScript/SkInvokedCoroutine.hpp>
#include <SkookumScript/SkParameterBase.hpp>
#include <SkookumScript/SkBoolean.hpp>
#include <SkookumScript/SkInteger.hpp>
#include <SkookumScript/SkReal.hpp>

//---------------------------------------------------------------------------------------

SkUEBlueprintInterface * SkUEBlueprintInterface::ms_singleton_p;

//---------------------------------------------------------------------------------------

SkUEBlueprintInterface::SkUEBlueprintInterface()
  {
  SK_ASSERTX(!ms_singleton_p, "There can be only one instance of this class.");
  ms_singleton_p = this;

  m_struct_vector3_p          = FindObjectChecked<UScriptStruct>(UObject::StaticClass()->GetOutermost(), TEXT("Vector"), false);
  m_struct_rotation_angles_p  = FindObjectChecked<UScriptStruct>(UObject::StaticClass()->GetOutermost(), TEXT("Rotator"), false);
  m_struct_transform_p        = FindObjectChecked<UScriptStruct>(UObject::StaticClass()->GetOutermost(), TEXT("Transform"), false);
  }

//---------------------------------------------------------------------------------------

SkUEBlueprintInterface::~SkUEBlueprintInterface()
  {
  clear();

  SK_ASSERTX_NO_THROW(ms_singleton_p == this, "There can be only one instance of this class.");
  ms_singleton_p = nullptr;
  }

//---------------------------------------------------------------------------------------

void SkUEBlueprintInterface::clear()
  {
  // Destroy all UFunctions and UProperties we allocated
  for (uint32_t i = 0; i < m_binding_entry_array.get_length(); ++i)
    {
    delete_binding_entry(i);
    }

  // And forget pointers to them 
  m_binding_entry_array.empty();
  }

//---------------------------------------------------------------------------------------

void SkUEBlueprintInterface::clear_all_sk_invokables()
  {
  for (BindingEntry * binding_entry_p : m_binding_entry_array)
    {
    binding_entry_p->m_sk_invokable_p = nullptr;
    }
  }

//---------------------------------------------------------------------------------------

UClass * SkUEBlueprintInterface::reexpose_class(SkClass * sk_class_p, tSkUEOnClassUpdatedFunc * on_class_updated_f, bool is_final)
  {
  UClass * ue_static_class_p = SkUEClassBindingHelper::get_static_ue_class_from_sk_class_super(sk_class_p);
  bool anything_changed = false;
  if (ue_static_class_p)
    {
    anything_changed = reexpose_class(sk_class_p, ue_static_class_p, on_class_updated_f, is_final);
    }

  // Clear parent class function cache if exists
  // as otherwise it might have cached a nullptr which might cause it to never find newly added functions
  #if WITH_EDITORONLY_DATA
    if (anything_changed)
      {
      UClass * ue_class_p = SkUEClassBindingHelper::get_ue_class_from_sk_class(sk_class_p);
      if (ue_class_p)
        {
        ue_class_p->ClearFunctionMapsCaches();
        }
      }
  #endif

  return ue_static_class_p;
  }

//---------------------------------------------------------------------------------------

bool SkUEBlueprintInterface::reexpose_class(SkClass * sk_class_p, UClass * ue_class_p, tSkUEOnClassUpdatedFunc * on_class_updated_f, bool is_final)
  {
  // Keep track of changes
  int32_t change_count = 0;

  // Find existing methods of this class and mark them for delete
  for (uint32_t i = 0; i < m_binding_entry_array.get_length(); ++i)
    {
    BindingEntry * binding_entry_p = m_binding_entry_array[i];
    if (binding_entry_p && binding_entry_p->m_sk_invokable_p->get_scope() == sk_class_p)
      {
      binding_entry_p->m_marked_for_delete = true;
      ++change_count;
      }
    }

  // Gather new methods/events
  for (auto method_p : sk_class_p->get_instance_methods())
    {
    change_count += int32_t(try_add_binding_entry(ue_class_p, method_p, is_final) >= 0);
    }
  for (auto method_p : sk_class_p->get_class_methods())
    {
    change_count += int32_t(try_add_binding_entry(ue_class_p, method_p, is_final) >= 0);
    }
  for (auto coroutine_p : sk_class_p->get_coroutines())
    {
    change_count += int32_t(try_add_binding_entry(ue_class_p, coroutine_p, is_final) >= 0);
    }

  // Now go and delete anything still marked for delete
  for (uint32_t i = 0; i < m_binding_entry_array.get_length(); ++i)
    {
    BindingEntry * binding_entry_p = m_binding_entry_array[i];
    if (binding_entry_p && binding_entry_p->m_marked_for_delete)
      {
      delete_binding_entry(i);
      }
    }

  // Invoke callback if anything changed
  if (on_class_updated_f && change_count > 0)
    {
    on_class_updated_f->invoke(ue_class_p);
    }

  return (change_count > 0);
  }

//---------------------------------------------------------------------------------------

void SkUEBlueprintInterface::reexpose_class_recursively(SkClass * sk_class_p, tSkUEOnClassUpdatedFunc * on_class_updated_f, bool is_final)
  {
  if (reexpose_class(sk_class_p, on_class_updated_f, is_final))
    {
    // Gather sub classes
    const tSkClasses & sub_classes = sk_class_p->get_subclasses();
    for (uint32_t i = 0; i < sub_classes.get_length(); ++i)
      {
      reexpose_class_recursively(sub_classes[i], on_class_updated_f, is_final);
      }
    }
  }

//---------------------------------------------------------------------------------------

void SkUEBlueprintInterface::reexpose_all(tSkUEOnClassUpdatedFunc * on_class_updated_f, bool is_final)
  {
  // Clear out old mappings
  clear();

  // Traverse Sk classes and gather methods that want to be exposed
  reexpose_class_recursively(SkUEEntity::get_class(), on_class_updated_f, is_final);
  }

//---------------------------------------------------------------------------------------

void SkUEBlueprintInterface::rebind_all_sk_invokables()
  {
  for (BindingEntry * binding_entry_p : m_binding_entry_array)
    {
    SkClass * sk_class_p = SkBrain::get_class(binding_entry_p->m_sk_class_name);
    SK_ASSERTX(sk_class_p, a_str_format("Could not find class `%s` while rebinding Blueprint exposed routines to new compiled binary.", binding_entry_p->m_sk_class_name.as_cstr()));
    if (sk_class_p)
      {
      SkInvokableBase * sk_invokable_p;
      if (binding_entry_p->m_is_class_member)
        {
        sk_invokable_p = sk_class_p->get_class_methods().get(binding_entry_p->m_sk_invokable_name);
        }
      else
        {
        sk_invokable_p = sk_class_p->get_instance_methods().get(binding_entry_p->m_sk_invokable_name);
        if (!sk_invokable_p)
          {
          sk_invokable_p = sk_class_p->get_coroutines().get(binding_entry_p->m_sk_invokable_name);
          }
        }
      SK_ASSERTX(sk_invokable_p, a_str_format("Could not find routine `%s@%s` while rebinding Blueprint exposed routines to new compiled binary.", binding_entry_p->m_sk_invokable_name.as_cstr(), binding_entry_p->m_sk_class_name.as_cstr()));
      binding_entry_p->m_sk_invokable_p = sk_invokable_p;
      }
    }
  }

//---------------------------------------------------------------------------------------

bool SkUEBlueprintInterface::is_skookum_blueprint_function(UFunction * function_p) const
  {
  Native native_function_p = function_p->GetNativeFunc();
  return native_function_p == (Native)&SkUEBlueprintInterface::exec_class_method
      || native_function_p == (Native)&SkUEBlueprintInterface::exec_instance_method
      || native_function_p == (Native)&SkUEBlueprintInterface::exec_coroutine;
  }

//---------------------------------------------------------------------------------------

bool SkUEBlueprintInterface::is_skookum_blueprint_event(UFunction * function_p) const
  {
  return function_p->RepOffset == EventMagicRepOffset;
  }

//---------------------------------------------------------------------------------------

void SkUEBlueprintInterface::exec_method(FFrame & stack, void * const result_p, SkClass * class_scope_p, SkInstance * this_p)
  {
  const FunctionEntry & function_entry = static_cast<const FunctionEntry &>(*ms_singleton_p->m_binding_entry_array[stack.CurrentNativeFunction->RepOffset]);
  SK_ASSERTX(function_entry.m_type == BindingType_Function, "BindingEntry has bad type!");
  SK_ASSERTX(function_entry.m_sk_invokable_p->get_invoke_type() == SkInvokable_method, "Must be a method at this point.");

  SkMethodBase * method_p = static_cast<SkMethodBase *>(function_entry.m_sk_invokable_p);
  if (method_p->get_scope() != class_scope_p)
    {
    method_p = static_cast<SkMethodBase *>(class_scope_p->get_invokable_from_vtable(this_p ? SkScope_instance : SkScope_class, method_p->get_vtable_index()));
  #if SKOOKUM & SK_DEBUG
    // If not found, might be due to recent live update and the vtable not being updated yet - try finding it by name
    if (method_p == nullptr || method_p->get_name() != function_entry.m_sk_invokable_p->get_name())
      {
      method_p = this_p 
        ? class_scope_p->find_instance_method_inherited(function_entry.m_sk_invokable_p->get_name()) 
        : class_scope_p->find_class_method_inherited(function_entry.m_sk_invokable_p->get_name());
      }
  #endif
    }
  SkInvokedMethod imethod(nullptr, this_p, method_p, a_stack_allocate(method_p->get_invoked_data_array_size(), SkInstance*));

  SKDEBUG_ICALL_SET_INTERNAL(&imethod);
  SKDEBUG_HOOK_SCRIPT_ENTRY(function_entry.m_sk_invokable_name);

  // Fill invoked method's argument list
  const SkParamEntry * param_entry_array = function_entry.get_param_entry_array();
  SK_ASSERTX(imethod.get_data().get_size() >= function_entry.m_num_params, a_str_format("Not enough space (%d) for %d arguments while invoking '%s@%s'!", imethod.get_data().get_size(), function_entry.m_num_params, function_entry.m_sk_class_name.as_cstr_dbg(), function_entry.m_sk_invokable_name.as_cstr_dbg()));
  for (uint32_t i = 0; i < function_entry.m_num_params; ++i)
    {
    const SkParamEntry & param_entry = param_entry_array[i];
    imethod.data_append_arg((*param_entry.m_fetcher_p)(stack, param_entry));
    }

  // Done with stack - now increment the code ptr unless it is null
  stack.Code += !!stack.Code;

  #if (SKOOKUM & SK_DEBUG)
    if (!this_p->get_class()->is_class(*function_entry.m_sk_invokable_p->get_scope()))
      {
      SK_ERRORX(a_str_format("Attempted to invoke method '%s@%s' via a blueprint of type '%s'. You might have forgotten to specify the SkookumScript type of this blueprint as '%s' in its SkookumScriptClassDataComponent.", function_entry.m_sk_class_name.as_cstr(), function_entry.m_sk_invokable_name.as_cstr(), this_p->get_class()->get_name_cstr(), function_entry.m_sk_class_name.as_cstr()));
      }
    else
  #endif
      {
      // Call method
      SkInstance * result_instance_p = SkBrain::ms_nil_p;
      static_cast<SkMethod *>(method_p)->SkMethod::invoke(&imethod, nullptr, &result_instance_p); // We know it's a method so call directly
      if (function_entry.m_result_getter)
        {
        (*function_entry.m_result_getter)(result_p, result_instance_p, function_entry.m_result_type);
        }
      }

  SKDEBUG_HOOK_SCRIPT_EXIT();
  }

//---------------------------------------------------------------------------------------

void SkUEBlueprintInterface::exec_class_method(FFrame & stack, void * const result_p)
  {
  SkClass * class_scope_p = SkUEClassBindingHelper::get_object_class((UObject *)this);
  exec_method(stack, result_p, class_scope_p, nullptr);
  }

//---------------------------------------------------------------------------------------

void SkUEBlueprintInterface::exec_instance_method(FFrame & stack, void * const result_p)
  {
  SkInstance * this_p = SkUEEntity::new_instance((UObject *)this);
  exec_method(stack, result_p, this_p->get_class(), this_p);
  this_p->dereference();
  }

//---------------------------------------------------------------------------------------

void SkUEBlueprintInterface::exec_coroutine(FFrame & stack, void * const result_p)
  {
  const FunctionEntry & function_entry = static_cast<const FunctionEntry &>(*ms_singleton_p->m_binding_entry_array[stack.CurrentNativeFunction->RepOffset]);
  SK_ASSERTX(function_entry.m_type == BindingType_Function, "BindingEntry has bad type!");
  SK_ASSERTX(function_entry.m_sk_invokable_p->get_invoke_type() == SkInvokable_coroutine, "Must be a coroutine at this point.");

  // Get instance of this object
  SkInstance * this_p = SkUEEntity::new_instance((UObject *)this);

  // Create invoked coroutine
  SkCoroutineBase * coro_p = static_cast<SkCoroutineBase *>(function_entry.m_sk_invokable_p);
  SkClass * class_scope_p = this_p->get_class();
  if (coro_p->get_scope() != class_scope_p)
    {
    coro_p = static_cast<SkCoroutine *>(class_scope_p->get_invokable_from_vtable_i(coro_p->get_vtable_index()));
  #if SKOOKUM & SK_DEBUG
    // If not found, might be due to recent live update and the vtable not being updated yet - try finding it by name
    if (coro_p == nullptr || coro_p->get_name() != function_entry.m_sk_invokable_p->get_name())
      {
      coro_p = class_scope_p->find_coroutine_inherited(function_entry.m_sk_invokable_p->get_name());
      }
  #endif
    }
  SkInvokedCoroutine * icoroutine_p = SkInvokedCoroutine::pool_new(coro_p);

  // Set parameters
  icoroutine_p->reset(SkCall_interval_always, nullptr, this_p, nullptr, nullptr);

  #if defined(SKDEBUG_COMMON)
    // Set with SKDEBUG_ICALL_STORE_GEXPR stored here before calls to argument expressions
    // overwrite it.
    const SkExpressionBase * call_expr_p = SkInvokedContextBase::ms_last_expr_p;
  #endif

  SKDEBUG_ICALL_SET_EXPR(icoroutine_p, call_expr_p);

  // Fill invoked coroutine's argument list
  const SkParamEntry * param_entry_array = function_entry.get_param_entry_array();
  icoroutine_p->data_ensure_size(function_entry.m_num_params);
  for (uint32_t i = 0; i < function_entry.m_num_params; ++i)
    {
    const SkParamEntry & param_entry = param_entry_array[i];
    icoroutine_p->data_append_arg((*param_entry.m_fetcher_p)(stack, param_entry));
    }

  // Done with stack - now increment the code ptr unless it is null
  stack.Code += !!stack.Code;

  SKDEBUG_HOOK_EXPR(call_expr_p, icoroutine_p, nullptr);

  #if (SKOOKUM & SK_DEBUG)
    if (!this_p->get_class()->is_class(*function_entry.m_sk_invokable_p->get_scope()))
      {
      SK_ERRORX(a_str_format("Attempted to invoke coroutine '%s@%s' via a blueprint of type '%s'. You might have forgotten to specify the SkookumScript type of this blueprint as '%s' in its SkookumScriptClassDataComponent.", function_entry.m_sk_class_name.as_cstr(), function_entry.m_sk_invokable_name.as_cstr(), this_p->get_class()->get_name_cstr(), function_entry.m_sk_class_name.as_cstr()));
      }
    else
  #endif
      {
      // Invoke the coroutine on this_p - might return immediately
      icoroutine_p->on_update();
      }

  // Free if not in use by our invoked coroutine
  this_p->dereference();
  }

//---------------------------------------------------------------------------------------
// Execute a blueprint event
void SkUEBlueprintInterface::mthd_trigger_event(SkInvokedMethod * scope_p, SkInstance ** result_pp)
  {
  const EventEntry & event_entry = static_cast<const EventEntry &>(*ms_singleton_p->m_binding_entry_array[scope_p->get_invokable()->get_user_data()]);
  SK_ASSERTX(event_entry.m_type == BindingType_Event, "BindingEntry has bad type!");

  // Create parameters on stack
  const K2ParamEntry * param_entry_array = event_entry.get_param_entry_array();
  UFunction * ue_function_p = event_entry.m_ue_function_p.Get(); // Invoke the first one
  uint8_t * k2_params_storage_p = a_stack_allocate(ue_function_p->ParmsSize, uint8_t);
  for (uint32_t i = 0; i < event_entry.m_num_params; ++i)
    {
    const K2ParamEntry & param_entry = param_entry_array[i];
    (*param_entry.m_getter_p)(k2_params_storage_p + param_entry.m_offset, scope_p->get_arg(i), param_entry);
    }

  // Invoke K2 script event with parameters
  AActor * actor_p = scope_p->this_as<SkUEActor>();
  if (!event_entry.m_ue_function_to_invoke_p.IsValid())
    {
    // Find Kismet copy of our method to invoke
    event_entry.m_ue_function_to_invoke_p = actor_p->FindFunctionChecked(*ue_function_p->GetName());
    }
  // Check if this event is actually present in any Blueprint graph
  SK_ASSERTX(event_entry.m_ue_function_to_invoke_p->Script.Num() > 0, a_str_format("Warning: Call to '%S' on actor '%S' has no effect as no Blueprint event node named '%S' exists in any of its event graphs.", *ue_function_p->GetName(), *actor_p->GetName(), *ue_function_p->GetName()));
  actor_p->ProcessEvent(event_entry.m_ue_function_to_invoke_p.Get(), k2_params_storage_p);

  // No return value
  if (result_pp) *result_pp = SkBrain::ms_nil_p;
  return;
  }

//---------------------------------------------------------------------------------------

template<class _TypedName>
bool SkUEBlueprintInterface::have_identical_signatures(const tSkParamList & param_list, const _TypedName * param_array_p)
  {
  for (uint32_t i = 0; i < param_list.get_length(); ++i)
    {
    const TypedName & typed_name = param_array_p[i];
    const SkParameterBase * param_p = param_list[i];
    if (typed_name.m_name != param_p->get_name()
     || typed_name.m_sk_class_p != param_p->get_expected_type()->get_key_class())
      {
      return false;
      }
    }

  return true;
  }

//---------------------------------------------------------------------------------------

bool SkUEBlueprintInterface::try_update_binding_entry(UClass * ue_class_p, SkInvokableBase * sk_invokable_p, int32_t * out_binding_index_p)
  {
  SK_ASSERTX(out_binding_index_p, "Must be non-null");

  const tSkParamList & param_list = sk_invokable_p->get_params().get_param_list();

  // See if we find any compatible entry already present:  
  for (int32_t binding_index = 0; binding_index < (int32_t)m_binding_entry_array.get_length(); ++binding_index)
    {
    BindingEntry * binding_entry_p = m_binding_entry_array[binding_index];
    if (binding_entry_p
      && binding_entry_p->m_sk_invokable_name == sk_invokable_p->get_name()
      && binding_entry_p->m_sk_class_name == sk_invokable_p->get_scope()->get_name()
      && binding_entry_p->m_is_class_member == sk_invokable_p->is_class_member())
      {
      // There is no overloading in SkookumScript
      // Therefore if the above matches we found our slot
      *out_binding_index_p = binding_index;

      // Don't update if UFunction is invalid or UClass no longer valid
      if (!binding_entry_p->m_ue_function_p.IsValid() || binding_entry_p->m_ue_function_p.Get()->GetOwnerClass() != ue_class_p)
        {
        return false;
        }

      // Can't update if signatures don't match
      if (binding_entry_p->m_num_params != param_list.get_length())
        {
        return false;
        }
      if (binding_entry_p->m_type == BindingType_Function)
        {
        FunctionEntry * function_entry_p = static_cast<FunctionEntry *>(binding_entry_p);
        if (!have_identical_signatures(param_list, function_entry_p->get_param_entry_array())
         || function_entry_p->m_result_type.m_sk_class_p != sk_invokable_p->get_params().get_result_class()->get_key_class())
          {
          return false;
          }
        }
      else
        {
        if (!have_identical_signatures(param_list, static_cast<EventEntry *>(binding_entry_p)->get_param_entry_array()))
          {
          return false;
          }

        // For events, remember which binding index to invoke
        sk_invokable_p->set_user_data(binding_index);
        }

      // We're good to update
      binding_entry_p->m_sk_invokable_p = sk_invokable_p; // Update Sk method pointer
      binding_entry_p->m_marked_for_delete = false; // Keep around
      return true; // Successfully updated
      }
    }

  // No matching entry found at all
  *out_binding_index_p = -1;
  return false;
  }

//---------------------------------------------------------------------------------------

int32_t SkUEBlueprintInterface::try_add_binding_entry(UClass * ue_class_p, SkInvokableBase * sk_invokable_p, bool is_final)
  {
  // Only look at methods that are annotated as blueprint
  if (sk_invokable_p->get_annotation_flags() & SkAnnotation_Blueprint)
    {
    // If it's a method with no body...
    if (sk_invokable_p->get_invoke_type() == SkInvokable_method_func
     || sk_invokable_p->get_invoke_type() == SkInvokable_method_mthd)
      { // ...it's an event
      return add_event_entry(ue_class_p, static_cast<SkMethodBase *>(sk_invokable_p), is_final);
      }
    else if (sk_invokable_p->get_invoke_type() == SkInvokable_method
          || sk_invokable_p->get_invoke_type() == SkInvokable_coroutine)
      { // ...otherwise it's a function/coroutine
      return add_function_entry(ue_class_p, sk_invokable_p, is_final);
      }
    else
      {
      SK_ERRORX(a_str_format("Trying to export coroutine %s to Blueprints which is atomic. Currently only scripted coroutines can be invoked via Blueprints.", sk_invokable_p->get_name_cstr()));
      }
    }

  return -1;
  }

//---------------------------------------------------------------------------------------

int32_t SkUEBlueprintInterface::add_function_entry(UClass * ue_class_p, SkInvokableBase * sk_invokable_p, bool is_final)
  {
  // Check if this binding already exists, and if so, just update it
  int32_t binding_index;
  if (try_update_binding_entry(ue_class_p, sk_invokable_p, &binding_index))
    {
    return binding_index;
    }
  if (binding_index >= 0)
    {
    delete_binding_entry(binding_index);
    }

  // Parameters of the method we are creating
  const SkParameters & params = sk_invokable_p->get_params();
  const tSkParamList & param_list = params.get_param_list();
  uint32_t num_params = param_list.get_length();

  // Create new UFunction
  ParamInfo * param_info_array_p = a_stack_allocate(num_params + 1, ParamInfo);
  UFunction * ue_function_p = build_ue_function(ue_class_p, sk_invokable_p, BindingType_Function, param_info_array_p, is_final);
  if (!ue_function_p) return -1;

  // Allocate binding entry
  const ParamInfo & return_info = param_info_array_p[num_params];
  bool has_return = return_info.m_ue_param_p != nullptr;
  FunctionEntry * function_entry_p = new(FMemory::Malloc(sizeof(FunctionEntry) + num_params * sizeof(SkParamEntry)))
    FunctionEntry(sk_invokable_p, ue_function_p, num_params, params.get_result_class()->get_key_class(), has_return ? return_info.m_ue_param_p->GetSize() : 0, return_info.m_sk_value_getter_p);

  // Initialize parameters
  for (uint32_t i = 0; i < num_params; ++i)
    {
    const SkParameterBase * input_param = param_list[i];
    const ParamInfo & param_info = param_info_array_p[i];
    new (&function_entry_p->get_param_entry_array()[i]) SkParamEntry(input_param->get_name(), param_info.m_ue_param_p->GetSize(), input_param->get_expected_type()->get_key_class(), param_info_array_p[i].m_k2_param_fetcher_p);
    }

  // Store binding entry in array
  binding_index = store_binding_entry(function_entry_p, binding_index);
  ue_function_p->RepOffset = uint16(binding_index); // Remember array index of method to call
  return binding_index;
  }

//---------------------------------------------------------------------------------------

int32_t SkUEBlueprintInterface::add_event_entry(UClass * ue_class_p, SkMethodBase * sk_method_p, bool is_final)
  {
  // Check if this binding already exists, and if so, just update it
  int32_t binding_index;
  if (try_update_binding_entry(ue_class_p, sk_method_p, &binding_index))
    {
    return binding_index;
    }
  if (binding_index >= 0)
    {
    delete_binding_entry(binding_index);
    }

  // Parameters of the method we are creating
  const SkParameters & params = sk_method_p->get_params();
  const tSkParamList & param_list = params.get_param_list();
  uint32_t num_params = param_list.get_length();

  // Create new UFunction
  ParamInfo * param_info_array_p = a_stack_allocate(num_params + 1, ParamInfo);
  UFunction * ue_function_p = build_ue_function(ue_class_p, sk_method_p, BindingType_Event, param_info_array_p, is_final);
  if (!ue_function_p) return -1;

  // Bind Sk method
  bind_event_method(sk_method_p);

  // Allocate binding entry
  EventEntry * event_entry_p = new(FMemory::Malloc(sizeof(EventEntry) + num_params * sizeof(K2ParamEntry))) EventEntry(sk_method_p, ue_function_p, num_params);

  // Initialize parameters
  for (uint32_t i = 0; i < num_params; ++i)
    {
    const SkParameterBase * input_param = param_list[i];
    const ParamInfo & param_info = param_info_array_p[i];
    new (&event_entry_p->get_param_entry_array()[i]) K2ParamEntry(input_param->get_name(), param_info.m_ue_param_p->GetSize(), input_param->get_expected_type()->get_key_class(), param_info.m_sk_value_getter_p, static_cast<UProperty *>(param_info.m_ue_param_p)->GetOffset_ForUFunction());
    }

  // Store binding entry in array
  return store_binding_entry(event_entry_p, binding_index);
  }

//---------------------------------------------------------------------------------------
// Store a given BindingEntry into the m_binding_entry_array
// If an index is given, use that, otherwise, find an empty slot to reuse, or if that fails, append a new entry
int32_t SkUEBlueprintInterface::store_binding_entry(BindingEntry * binding_entry_p, int32_t binding_index_to_use)
  {
  // If no binding index known yet, look if there is an empty slot that we can reuse
  if (binding_index_to_use < 0)
    {
    for (binding_index_to_use = 0; binding_index_to_use < (int32_t)m_binding_entry_array.get_length(); ++binding_index_to_use)
      {
      if (!m_binding_entry_array[binding_index_to_use]) break;
      }
    }
  if (binding_index_to_use == m_binding_entry_array.get_length())
    {
    m_binding_entry_array.append(*binding_entry_p);
    }
  else
    {
    m_binding_entry_array.set_at(binding_index_to_use, binding_entry_p);
    }

  // Remember binding index to invoke Blueprint events
  binding_entry_p->m_sk_invokable_p->set_user_data(binding_index_to_use);

  return binding_index_to_use;
  }

//---------------------------------------------------------------------------------------
// Delete binding entry and set pointer to nullptr so it can be reused

void SkUEBlueprintInterface::delete_binding_entry(uint32_t binding_index)
  {
  BindingEntry * binding_entry_p = m_binding_entry_array[binding_index];
  if (binding_entry_p)
    {
    SK_ASSERTX(binding_entry_p->m_ue_function_p.IsValid() || !binding_entry_p->m_ue_class_p.IsValid(), a_str_format("UFunction %s was deleted outside of SkUEBlueprintInterface and left dangling links behind in its owner UClass (%S).", binding_entry_p->m_sk_invokable_name.as_cstr(), *binding_entry_p->m_ue_class_p->GetName()));
    if (binding_entry_p->m_ue_function_p.IsValid())
      {
      UFunction * ue_function_p = binding_entry_p->m_ue_function_p.Get();
      // Detach from class if exists
      if (binding_entry_p->m_ue_class_p.IsValid())
        {
        UClass * ue_class_p = binding_entry_p->m_ue_class_p.Get();
        // Unlink from its owner class
        ue_class_p->RemoveFunctionFromFunctionMap(ue_function_p);
        // Unlink from the Children list as well
        UField ** prev_field_pp = &ue_class_p->Children;
        for (UField * field_p = *prev_field_pp; field_p; prev_field_pp = &field_p->Next, field_p = *prev_field_pp)
          {
          if (field_p == ue_function_p)
            {
            *prev_field_pp = field_p->Next;
            break;
            }
          }
        }
      // Destroy the function along with its attached properties
      // HACK remove from root if it's rooted - proper fix: Find out why it's rooted to begin with
      if (ue_function_p->IsRooted())
        {
        ue_function_p->RemoveFromRoot();
        }
      ue_function_p->MarkPendingKill();
      }
    FMemory::Free(binding_entry_p);
    m_binding_entry_array.set_at(binding_index, nullptr);
    }
  }

//---------------------------------------------------------------------------------------
// Params:
//   out_param_info_array_p: Storage for info on each parameter, return value is stored behind the input parameters, and is zeroed if there is no return value
UFunction * SkUEBlueprintInterface::build_ue_function(UClass * ue_class_p, SkInvokableBase * sk_invokable_p, eBindingType binding_type, ParamInfo * out_param_info_array_p, bool is_final)
  {
  // Build name of method including scope
  const char * invokable_name_p = sk_invokable_p->get_name_cstr();
  const char * class_name_p = sk_invokable_p->get_scope()->get_name_cstr();
  AString qualified_invokable_name;
  qualified_invokable_name.ensure_size_buffer(uint32_t(::strlen(invokable_name_p) + ::strlen(class_name_p) + 3u));
  qualified_invokable_name.append(class_name_p);
  qualified_invokable_name.append(" @ ", 3u);
  qualified_invokable_name.append(invokable_name_p);
  FName qualified_invokable_fname(qualified_invokable_name.as_cstr());

  // Must not be already present
  check(!ue_class_p->FindFunctionByName(qualified_invokable_fname));

  // Make UFunction object
  UFunction * ue_function_p = NewObject<UFunction>(ue_class_p, qualified_invokable_fname, RF_Public);

  ue_function_p->FunctionFlags |= FUNC_Public;
  if (sk_invokable_p->is_class_member())
    {
    ue_function_p->FunctionFlags |= FUNC_Static;
    }

  if (binding_type == BindingType_Function)
    {
    ue_function_p->FunctionFlags |= FUNC_BlueprintCallable | FUNC_Native;
    ue_function_p->SetNativeFunc(sk_invokable_p->get_invoke_type() == SkInvokable_coroutine 
      ? (Native)&SkUEBlueprintInterface::exec_coroutine
      : (sk_invokable_p->is_class_member() ? (Native)&SkUEBlueprintInterface::exec_class_method : (Native)&SkUEBlueprintInterface::exec_instance_method));
    #if WITH_EDITOR
      ue_function_p->SetMetaData(TEXT("Tooltip"), *FString::Printf(TEXT("%S\n%S@%S()"), 
        sk_invokable_p->get_invoke_type() == SkInvokable_coroutine ? "Kick off SkookumScript coroutine" : "Call to SkookumScript method", 
        sk_invokable_p->get_scope()->get_name_cstr(), 
        sk_invokable_p->get_name_cstr()));
    #endif
    }
  else // binding_type == BindingType_Event
    {
    ue_function_p->FunctionFlags |= FUNC_BlueprintEvent | FUNC_Event;
    ue_function_p->Bind(); // Bind to default Blueprint event mechanism
    #if WITH_EDITOR
      ue_function_p->SetMetaData(TEXT("Tooltip"), *FString::Printf(TEXT("Triggered by SkookumScript method\n%S@%S()"), sk_invokable_p->get_scope()->get_name_cstr(), sk_invokable_p->get_name_cstr()));
    #endif    
    ue_function_p->RepOffset = EventMagicRepOffset; // So we can tell later this is an Sk event
    }

  #if WITH_EDITOR
    ue_function_p->SetMetaData(TEXT("Category"), TEXT("SkookumScript"));
  #endif

  // Parameters of the method we are creating
  const SkParameters & params = sk_invokable_p->get_params();
  const tSkParamList & param_list = params.get_param_list();
  uint32_t num_params = param_list.get_length();

  // Handle return value if any
  if (params.get_result_class() && params.get_result_class() != SkBrain::ms_object_class_p)
    {
    UProperty * result_param_p = build_ue_param(ue_function_p, params.get_result_class(), "result", out_param_info_array_p ? out_param_info_array_p + num_params : nullptr, is_final);
    if (!result_param_p)
      {
      // If any parameters can not be mapped, skip building this entire function
      ue_function_p->MarkPendingKill();
      return nullptr;
      }

    result_param_p->PropertyFlags |= CPF_ReturnParm; // Flag as return value
    }
  else if (out_param_info_array_p)
    {
    // If there is no return value, indicate that by zeroing that entry in the param info array
    memset(out_param_info_array_p + num_params, 0, sizeof(*out_param_info_array_p));
    }

  // Handle input parameters (in reverse order so they get linked into the list in proper order)
  for (int32_t i = num_params - 1; i >= 0; --i)
    {
    const SkParameterBase * input_param = param_list[i];
    if (!build_ue_param(ue_function_p, input_param->get_expected_type(), input_param->get_name_cstr(), out_param_info_array_p ? out_param_info_array_p + i : nullptr, is_final))
      {
      // If any parameters can not be mapped, skip building this entire function
      ue_function_p->MarkPendingKill();
      return nullptr;
      }
    }

  // Make method known to its class
  ue_class_p->LinkChild(ue_function_p);
  ue_class_p->AddFunctionToFunctionMap(ue_function_p);

  // Make sure parameter list is properly linked and offsets are set
  ue_function_p->StaticLink(true);

  return ue_function_p;
  }

//---------------------------------------------------------------------------------------

UProperty * SkUEBlueprintInterface::build_ue_param(UFunction * ue_function_p, SkClassDescBase * sk_parameter_class_p, const FName & param_name, ParamInfo * out_param_info_p, bool is_final)
  {
  // Based on Sk type, figure out the matching UProperty as well as fetcher and setter methods
  UProperty * property_p = nullptr;
  tK2ParamFetcher k2_param_fetcher_p = nullptr;
  tSkValueGetter sk_value_getter_p = nullptr;
  if (sk_parameter_class_p == SkBoolean::get_class())
    {
    property_p = NewObject<UBoolProperty>(ue_function_p, param_name, RF_Public);
    k2_param_fetcher_p = &fetch_k2_param_boolean;
    sk_value_getter_p = &get_sk_value_boolean;
    }
  else if (sk_parameter_class_p == SkInteger::get_class())
    {
    property_p = NewObject<UIntProperty>(ue_function_p, param_name, RF_Public);
    k2_param_fetcher_p = &fetch_k2_param_integer;
    sk_value_getter_p = &get_sk_value_integer;
    }
  else if (sk_parameter_class_p == SkReal::get_class())
    {
    property_p = NewObject<UFloatProperty>(ue_function_p, param_name, RF_Public);
    k2_param_fetcher_p = &fetch_k2_param_real;
    sk_value_getter_p = &get_sk_value_real;
    }
  else if (sk_parameter_class_p == SkString::get_class())
    {
    property_p = NewObject<UStrProperty>(ue_function_p, param_name, RF_Public);
    k2_param_fetcher_p = &fetch_k2_param_string;
    sk_value_getter_p = &get_sk_value_string;
    }
  else if (sk_parameter_class_p == SkVector3::get_class())
    {
    UStructProperty * struct_property_p = NewObject<UStructProperty>(ue_function_p, param_name);
    struct_property_p->Struct = m_struct_vector3_p;
    property_p = struct_property_p;
    k2_param_fetcher_p = &fetch_k2_param_vector3;
    sk_value_getter_p = &get_sk_value_vector3;
    }
  else if (sk_parameter_class_p == SkRotationAngles::get_class())
    {
    UStructProperty * struct_property_p = NewObject<UStructProperty>(ue_function_p, param_name);
    struct_property_p->Struct = m_struct_rotation_angles_p;
    property_p = struct_property_p;
    k2_param_fetcher_p = &fetch_k2_param_rotation_angles;
    sk_value_getter_p = &get_sk_value_rotation_angles;
    }
  else if (sk_parameter_class_p == SkTransform::get_class())
    {
    UStructProperty * struct_property_p = NewObject<UStructProperty>(ue_function_p, param_name);
    struct_property_p->Struct = m_struct_transform_p;
    property_p = struct_property_p;
    k2_param_fetcher_p = &fetch_k2_param_transform;
    sk_value_getter_p = &get_sk_value_transform;
    }
  else if (sk_parameter_class_p->get_key_class()->is_class(*SkUEEntity::get_class()))
    {
    UClass * uclass_p;
    bool is_superclass = false;
    A_LOOP_INFINITE
      {
      uclass_p = SkUEClassBindingHelper::get_ue_class_from_sk_class(sk_parameter_class_p);
      SK_ASSERTX(uclass_p || is_superclass || !is_final, a_cstr_format("Class '%s' of parameter '%s' of method '%S.%S' being exported to Blueprints is not a known engine class. Maybe it is the class of a Blueprint that is not loaded yet?", sk_parameter_class_p->get_key_class_name().as_cstr_dbg(), param_name.GetPlainANSIString(), *ue_function_p->GetOwnerClass()->GetName(), *ue_function_p->GetName()));
      if (uclass_p || sk_parameter_class_p == SkUEEntity::get_class()) break;
      // If not final, keep looking for existing super classes as temp replacement just to get the Blueprint to compile
      sk_parameter_class_p = sk_parameter_class_p->get_key_class()->get_superclass();
      is_superclass = true;
      }
    if (uclass_p)
      {
      property_p = NewObject<UObjectProperty>(ue_function_p, param_name, RF_Public);
      static_cast<UObjectProperty *>(property_p)->PropertyClass = uclass_p;
      k2_param_fetcher_p = &fetch_k2_param_entity;
      sk_value_getter_p = &get_sk_value_entity;
      }
    }
  else
    {
    UStruct * ustruct_p;
    bool is_superclass = false;
    A_LOOP_INFINITE
      {
      ustruct_p = SkUEClassBindingHelper::get_static_ue_struct_from_sk_class(sk_parameter_class_p);    
      SK_ASSERTX(ustruct_p || is_superclass || !is_final, a_cstr_format("Class '%s' of parameter '%s' of method '%S.%S' being exported to Blueprints can not be mapped to a Blueprint-compatible type.", sk_parameter_class_p->get_key_class_name().as_cstr_dbg(), param_name.GetPlainANSIString(), *ue_function_p->GetOwnerClass()->GetName(), *ue_function_p->GetName()));
      if (ustruct_p || sk_parameter_class_p == SkUEEntity::get_class()) break;
      // If not final, keep looking for existing super classes as temp replacement just to get the Blueprint to compile
      sk_parameter_class_p = sk_parameter_class_p->get_key_class()->get_superclass();
      is_superclass = true;
      }
    if (ustruct_p)
      {
      property_p = NewObject<UStructProperty>(ue_function_p, param_name);
      static_cast<UStructProperty *>(property_p)->Struct = CastChecked<UScriptStruct>(ustruct_p);
      if (SkInstance::is_data_stored_by_val(ustruct_p->GetStructureSize()))
        {
        k2_param_fetcher_p = &fetch_k2_param_struct_val;
        sk_value_getter_p = &get_sk_value_struct_val;
        }
      else
        {
        k2_param_fetcher_p = &fetch_k2_param_struct_ref;
        sk_value_getter_p = &get_sk_value_struct_ref;
        }
      }
    }

  // Add flags
  if (property_p)
    {
    property_p->PropertyFlags |= CPF_Parm;
    ue_function_p->LinkChild(property_p);
    }

  // Set result
  if (out_param_info_p)
    {
    out_param_info_p->m_ue_param_p = property_p;
    out_param_info_p->m_k2_param_fetcher_p = k2_param_fetcher_p;
    out_param_info_p->m_sk_value_getter_p = sk_value_getter_p;
    }

  return property_p;
  }

//---------------------------------------------------------------------------------------

void SkUEBlueprintInterface::bind_event_method(SkMethodBase * sk_method_p)
  {
  SK_ASSERTX(!sk_method_p->is_bound() || static_cast<SkMethodFunc *>(sk_method_p)->m_atomic_f == &mthd_trigger_event, a_str_format("Trying to bind Blueprint event method '%s' but it is already bound to a different atomic implementation!", sk_method_p->get_name_cstr_dbg()));
  if (!sk_method_p->is_bound())
    {
    sk_method_p->get_scope()->register_method_func(sk_method_p->get_name(), &mthd_trigger_event, sk_method_p->is_class_member() ? SkBindFlag_class_no_rebind : SkBindFlag_instance_no_rebind);
    }
  }

//---------------------------------------------------------------------------------------

SkInstance * SkUEBlueprintInterface::fetch_k2_param_boolean(FFrame & stack, const TypedName & typed_name)
  {
  UBoolProperty::TCppType value = UBoolProperty::GetDefaultPropertyValue();
  stack.StepCompiledIn<UBoolProperty>(&value);
  return SkBoolean::new_instance(value);
  }

//---------------------------------------------------------------------------------------

SkInstance * SkUEBlueprintInterface::fetch_k2_param_integer(FFrame & stack, const TypedName & typed_name)
  {
  UIntProperty::TCppType value = UIntProperty::GetDefaultPropertyValue();
  stack.StepCompiledIn<UIntProperty>(&value);
  return SkInteger::new_instance(value);
  }

//---------------------------------------------------------------------------------------

SkInstance * SkUEBlueprintInterface::fetch_k2_param_real(FFrame & stack, const TypedName & typed_name)
  {
  UFloatProperty::TCppType value = UFloatProperty::GetDefaultPropertyValue();
  stack.StepCompiledIn<UFloatProperty>(&value);
  return SkReal::new_instance(value);
  }

//---------------------------------------------------------------------------------------

SkInstance * SkUEBlueprintInterface::fetch_k2_param_string(FFrame & stack, const TypedName & typed_name)
  {
  UStrProperty::TCppType value = UStrProperty::GetDefaultPropertyValue();
  stack.StepCompiledIn<UStrProperty>(&value);
  return SkString::new_instance(FStringToAString(value));
  }

//---------------------------------------------------------------------------------------

SkInstance * SkUEBlueprintInterface::fetch_k2_param_vector3(FFrame & stack, const TypedName & typed_name)
  {
  FVector value(ForceInitToZero);
  stack.StepCompiledIn<UStructProperty>(&value);
  return SkVector3::new_instance(value);
  }

//---------------------------------------------------------------------------------------

SkInstance * SkUEBlueprintInterface::fetch_k2_param_rotation_angles(FFrame & stack, const TypedName & typed_name)
  {
  FRotator value(ForceInitToZero);
  stack.StepCompiledIn<UStructProperty>(&value);
  return SkRotationAngles::new_instance(value);
  }

//---------------------------------------------------------------------------------------

SkInstance * SkUEBlueprintInterface::fetch_k2_param_transform(FFrame & stack, const TypedName & typed_name)
  {
  FTransform value;
  stack.StepCompiledIn<UStructProperty>(&value);
  return SkTransform::new_instance(value);
  }

//---------------------------------------------------------------------------------------

SkInstance * SkUEBlueprintInterface::fetch_k2_param_struct_val(FFrame & stack, const TypedName & typed_name)
  {
  void * user_data_p;
  SkInstance * instance_p = SkInstance::new_instance_uninitialized_val(typed_name.m_sk_class_p, typed_name.m_byte_size, &user_data_p);
  stack.StepCompiledIn<UStructProperty>(user_data_p);
  return instance_p;
  }

//---------------------------------------------------------------------------------------

SkInstance * SkUEBlueprintInterface::fetch_k2_param_struct_ref(FFrame & stack, const TypedName & typed_name)
  {
  void * user_data_p;
  SkInstance * instance_p = SkInstance::new_instance_uninitialized_ref(typed_name.m_sk_class_p, typed_name.m_byte_size, &user_data_p);
  stack.StepCompiledIn<UStructProperty>(user_data_p);
  return instance_p;
  }

//---------------------------------------------------------------------------------------

SkInstance * SkUEBlueprintInterface::fetch_k2_param_entity(FFrame & stack, const TypedName & typed_name)
  {
  UObject * obj_p = nullptr;
  stack.StepCompiledIn<UObjectPropertyBase>(&obj_p);
  return SkUEEntity::new_instance(obj_p);
  }

//---------------------------------------------------------------------------------------

uint32_t SkUEBlueprintInterface::get_sk_value_boolean(void * const result_p, SkInstance * value_p, const TypedName & typed_name)
  {
  *((UBoolProperty::TCppType *)result_p) = value_p->as<SkBoolean>();
  return sizeof(UBoolProperty::TCppType);
  }

//---------------------------------------------------------------------------------------

uint32_t SkUEBlueprintInterface::get_sk_value_integer(void * const result_p, SkInstance * value_p, const TypedName & typed_name)
  {
  *((UIntProperty::TCppType *)result_p) = value_p->as<SkInteger>();
  return sizeof(UIntProperty::TCppType);
  }

//---------------------------------------------------------------------------------------

uint32_t SkUEBlueprintInterface::get_sk_value_real(void * const result_p, SkInstance * value_p, const TypedName & typed_name)
  {
  *((UFloatProperty::TCppType *)result_p) = value_p->as<SkReal>();
  return sizeof(UFloatProperty::TCppType);
  }

//---------------------------------------------------------------------------------------

uint32_t SkUEBlueprintInterface::get_sk_value_string(void * const result_p, SkInstance * value_p, const TypedName & typed_name)
  {
  *((UStrProperty::TCppType *)result_p) = AStringToFString(value_p->as<SkString>());
  return sizeof(UStrProperty::TCppType);
  }

//---------------------------------------------------------------------------------------

uint32_t SkUEBlueprintInterface::get_sk_value_vector3(void * const result_p, SkInstance * value_p, const TypedName & typed_name)
  {
  *((FVector *)result_p) = value_p->as<SkVector3>();
  return sizeof(FVector);
  }

//---------------------------------------------------------------------------------------

uint32_t SkUEBlueprintInterface::get_sk_value_rotation_angles(void * const result_p, SkInstance * value_p, const TypedName & typed_name)
  {
  *((FRotator *)result_p) = value_p->as<SkRotationAngles>();
  return sizeof(FRotator);
  }

//---------------------------------------------------------------------------------------

uint32_t SkUEBlueprintInterface::get_sk_value_transform(void * const result_p, SkInstance * value_p, const TypedName & typed_name)
  {
  *((FTransform *)result_p) = value_p->as<SkTransform>();
  return sizeof(FTransform);
  }

//---------------------------------------------------------------------------------------

uint32_t SkUEBlueprintInterface::get_sk_value_struct_val(void * const result_p, SkInstance * value_p, const TypedName & typed_name)
  {
  // Cast to uint32_t* hoping the compiler will get the hint and optimize the copy
  ::memcpy(reinterpret_cast<uint32_t *>(result_p), reinterpret_cast<uint32_t *>(SkInstance::get_raw_pointer_val(value_p)), typed_name.m_byte_size);
  return typed_name.m_byte_size;
  }

//---------------------------------------------------------------------------------------

uint32_t SkUEBlueprintInterface::get_sk_value_struct_ref(void * const result_p, SkInstance * value_p, const TypedName & typed_name)
  {
  // Cast to uint32_t* hoping the compiler will get the hint and optimize the copy
  ::memcpy(reinterpret_cast<uint32_t *>(result_p), reinterpret_cast<uint32_t *>(SkInstance::get_raw_pointer_ref(value_p)), typed_name.m_byte_size);
  return typed_name.m_byte_size;
  }

//---------------------------------------------------------------------------------------

uint32_t SkUEBlueprintInterface::get_sk_value_entity(void * const result_p, SkInstance * value_p, const TypedName & typed_name)
  {
  *((UObject **)result_p) = value_p->as<SkUEEntity>();
  return sizeof(UObject *);
  }
