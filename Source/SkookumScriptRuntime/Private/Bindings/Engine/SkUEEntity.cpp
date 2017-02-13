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
//
// Additional bindings for the Entity (= UObject) class 
//=======================================================================================

//=======================================================================================
// Includes
//=======================================================================================

#include "../../SkookumScriptRuntimePrivatePCH.h"
#include "SkUEEntity.hpp"
#include "SkUEEntityClass.hpp"
#include "SkUEName.hpp"
#include "../SkUEUtils.hpp"
#include <SkUEEntityClass.generated.hpp>

#include "Engine/World.h"

#include <SkookumScript/SkBoolean.hpp>
#include <SkookumScript/SkInteger.hpp>
#include <SkookumScript/SkInvokedCoroutine.hpp>

//---------------------------------------------------------------------------------------

namespace SkUEEntity_Impl
  {

  //---------------------------------------------------------------------------------------
  // !new constructor - creates new object
  void mthd_ctor_new(SkInvokedMethod * scope_p, SkInstance ** result_pp)
    {
    UObject * outer_p = scope_p->get_arg<SkUEEntity>(SkArg_1);
    FName name = scope_p->get_arg<SkUEName>(SkArg_2);
    uint32_t flags = scope_p->get_arg<SkInteger>(SkArg_3);
    
    // The scope of a constructor is always some form of an SkInstance
    SkInstance * receiver_p = static_cast<SkInstance *>(scope_p->m_scope_p.get_obj());

    UClass * ue_class_p = SkUEClassBindingHelper::get_ue_class_from_sk_class(receiver_p->get_class());
    SK_ASSERTX(ue_class_p, a_cstr_format("The UE4 equivalent of class type '%s' is not known to SkookumScript. Maybe it is the class of a Blueprint that is not loaded yet?", receiver_p->get_class()->get_name_cstr_dbg()));
    scope_p->get_this()->construct<SkUEEntity>(NewObject<UObject>(outer_p, ue_class_p, name, EObjectFlags(flags)));
    }

  //---------------------------------------------------------------------------------------
  // !copy constructor - copies class pointer as well
  void mthd_ctor_copy(SkInvokedMethod * scope_p, SkInstance ** result_pp)
    {
    SkInstance * this_p = scope_p->get_this();
    SkInstance * other_p = scope_p->get_arg(SkArg_1);
    this_p->construct<SkUEEntity>(other_p->as<SkUEEntity>());
    this_p->set_class(other_p->get_class());
    }

  //---------------------------------------------------------------------------------------
  // Null constructor - constructs empty object
  void mthd_ctor_null(SkInvokedMethod * scope_p, SkInstance ** result_pp)
    {
    SkInstance * this_p = scope_p->get_this();
    this_p->construct<SkUEEntity>();
    }

  //---------------------------------------------------------------------------------------
  // Assignment operator - copies class pointer as well
  void mthd_op_assign(SkInvokedMethod * scope_p, SkInstance ** result_pp)
    {
    SkInstance * this_p = scope_p->get_this();
    SkInstance * other_p = scope_p->get_arg(SkArg_1);
    this_p->as<SkUEEntity>() = other_p->as<SkUEEntity>();
    this_p->set_class(other_p->get_class());

    // Return this if result desired
    if (result_pp)
      {
      this_p->reference();
      *result_pp = this_p;
      }
    }

  //---------------------------------------------------------------------------------------
  // Tests if entity is null
  static void mthd_null_Q(SkInvokedMethod * scope_p, SkInstance ** result_pp)
    {
    if (result_pp) // Do nothing if result not desired
      {
      *result_pp = SkBoolean::new_instance(!scope_p->this_as<SkUEEntity>().is_valid());
      }
    }

  //---------------------------------------------------------------------------------------
  // Convert to String
  static void mthd_String(SkInvokedMethod * scope_p, SkInstance ** result_pp)
    {    
    if (result_pp) // Do nothing if result not desired
      {
      UObject * this_p = scope_p->this_as<SkUEEntity>();
      AString obj_name = this_p ? FStringToAString(this_p->GetName()) : "null";
      AString class_name = scope_p->get_this()->get_class()->get_name().as_string();
      AString uclass_name = this_p ? FStringToAString(this_p->GetClass()->GetName()) : "null";

      AString str(nullptr, 9u + obj_name.get_length() + class_name.get_length() + uclass_name.get_length(), 0u);
      str.append('"');
      str.append(obj_name);
      str.append("\" <", 3u);
      str.append(class_name);
      str.append("> (", 3u);
      str.append(uclass_name);
      str.append(')');
      *result_pp = SkString::new_instance(str);
      }
    }

  //---------------------------------------------------------------------------------------
  // Get name of this Entity
  static void mthd_name(SkInvokedMethod * scope_p, SkInstance ** result_pp)
    {    
    if (result_pp) // Do nothing if result not desired
      {
      UObject * this_p = scope_p->this_as<SkUEEntity>();
      AString obj_name = this_p ? FStringToAString(this_p->GetName()) : "null";
      *result_pp = SkString::new_instance(obj_name);
      }
    }

  //---------------------------------------------------------------------------------------
  // Get Unreal Engine class of this Entity
  static void mthd_entity_class(SkInvokedMethod * scope_p, SkInstance ** result_pp)
    {
    if (result_pp) // Do nothing if result not desired
      {
      UObject * this_p = scope_p->this_as<SkUEEntity>();
      *result_pp = this_p ? SkUEEntityClass::new_instance(this_p->GetClass()) : SkBrain::ms_nil_p;
      }
    }

  //---------------------------------------------------------------------------------------
  // =(Entity operand) Boolean
  static void mthd_op_equals(SkInvokedMethod * scope_p, SkInstance ** result_pp)
    {
    if (result_pp) // Do nothing if result not desired
      {
      *result_pp = SkBoolean::new_instance(scope_p->this_as<SkUEEntity>() == scope_p->get_arg<SkUEEntity>(SkArg_1));
      }
    }

  //---------------------------------------------------------------------------------------
  // ~=(Entity operand) Boolean
  static void mthd_op_not_equal(SkInvokedMethod * scope_p, SkInstance ** result_pp)
    {
    if (result_pp) // Do nothing if result not desired
      {
      *result_pp = SkBoolean::new_instance(scope_p->this_as<SkUEEntity>() != scope_p->get_arg<SkUEEntity>(SkArg_1));
      }
    }

  //---------------------------------------------------------------------------------------
  // Get Unreal Engine class of this class
  static void mthdc_static_class(SkInvokedMethod * scope_p, SkInstance ** result_pp)
    {
    if (result_pp) // Do nothing if result not desired
      {
      // Determine class
      SkClass * class_p = ((SkMetaClass *)scope_p->get_topmost_scope())->get_class_info();
      UClass * uclass_p = SkUEClassBindingHelper::get_ue_class_from_sk_class(class_p);
      SK_ASSERTX(uclass_p, a_cstr_format("The UE4 equivalent of class type '%s' is not known to SkookumScript. Maybe it is the class of a Blueprint that is not loaded yet?", class_p->get_name_cstr_dbg()));
      *result_pp = uclass_p ? SkUEEntityClass::new_instance(uclass_p) : SkBrain::ms_nil_p;
      }
    }

  //---------------------------------------------------------------------------------------
  // Entity@load(String name) ThisClass_
  static void mthdc_load(SkInvokedMethod * scope_p, SkInstance ** result_pp)
    {
    if (result_pp) // Do nothing if result not desired
      {
      // Determine class of object to load
      SkClass * class_p = ((SkMetaClass *)scope_p->get_topmost_scope())->get_class_info();
      UClass * uclass_p = SkUEClassBindingHelper::get_ue_class_from_sk_class(class_p);
      SK_ASSERTX(uclass_p, a_cstr_format("Cannot load entity '%s' as the UE4 equivalent of class type '%s' is not known to SkookumScript. Maybe it is the class of a Blueprint that is not loaded yet?", scope_p->get_arg<SkString>(SkArg_1).as_cstr(), class_p->get_name_cstr_dbg()));

      // Load object
      UObject * obj_p = nullptr;
      if (uclass_p)
        {
        obj_p = StaticLoadObject(uclass_p, SkUEClassBindingHelper::get_world(), *AStringToFString(scope_p->get_arg<SkString>(SkArg_1)));
        }

      *result_pp = obj_p ? SkUEEntity::new_instance(obj_p, uclass_p, class_p) : SkBrain::ms_nil_p;
      }
    }

  //---------------------------------------------------------------------------------------
  // Entity@default() ThisClass_
  static void mthdc_default(SkInvokedMethod * scope_p, SkInstance ** result_pp)
    {
    if (result_pp) // Do nothing if result not desired
      {
      // Determine class of object to get
      SkClass * class_p = ((SkMetaClass *)scope_p->get_topmost_scope())->get_class_info();
      UClass * uclass_p = SkUEClassBindingHelper::get_ue_class_from_sk_class(class_p);
      SK_ASSERTX(uclass_p, a_cstr_format("Cannot get default instance of class '%s' as the UE4 equivalent of this class is not known to SkookumScript. Maybe it is the class of a Blueprint that is not loaded yet?", class_p->get_name_cstr_dbg()));

      // Get default object
      UObject * obj_p = nullptr;
      if (uclass_p)
        {
        obj_p = GetMutableDefault<UObject>(uclass_p);
        }

      *result_pp = obj_p ? SkUEEntity::new_instance(obj_p, uclass_p, class_p) : SkBrain::ms_nil_p;
      }
    }

  //---------------------------------------------------------------------------------------
  // Entity@_wait_until_destroyed()
  static bool coro_wait_until_destroyed(SkInvokedCoroutine * scope_p)
    {
    UObject * this_p = scope_p->this_as<SkUEEntity>(); // We store the UObject as a weak pointer so it becomes null when the object is destroyed
    return !this_p || !this_p->IsValidLowLevel();
    }

  static const SkClass::MethodInitializerFunc methods_i2[] =
    {
      { "!new",         mthd_ctor_new },
      { "!copy",        mthd_ctor_copy },
      { "!null",        mthd_ctor_null },
      { "assign",       mthd_op_assign },
      { "null?",        mthd_null_Q },
      { "String",       mthd_String },
      { "name",         mthd_name },
      { "entity_class", mthd_entity_class },
      { "equal?",       mthd_op_equals },
      { "not_equal?",   mthd_op_not_equal },
    };

  static const SkClass::MethodInitializerFunc methods_c2[] =
    {
      { "static_class", mthdc_static_class },
      { "load",         mthdc_load },
      { "default",      mthdc_default },
    };

  } // SkUEEntity_Impl

//---------------------------------------------------------------------------------------

void SkUEEntity_Ext::register_bindings()
  {
  ms_class_p->register_method_func_bulk(SkUEEntity_Impl::methods_i2, A_COUNT_OF(SkUEEntity_Impl::methods_i2), SkBindFlag_instance_no_rebind);
  ms_class_p->register_method_func_bulk(SkUEEntity_Impl::methods_c2, A_COUNT_OF(SkUEEntity_Impl::methods_c2), SkBindFlag_class_no_rebind);
  ms_class_p->register_coroutine_func("_wait_until_destroyed", SkUEEntity_Impl::coro_wait_until_destroyed, SkBindFlag_instance_no_rebind);
  }

