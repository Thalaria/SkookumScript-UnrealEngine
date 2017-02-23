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
// Bindings for the SkookumScriptBehaviorComponent class
//=======================================================================================

//=======================================================================================
// Includes
//=======================================================================================

#include "../../SkookumScriptRuntimePrivatePCH.h"
#include "SkookumScriptBehaviorComponent.h"
#include "SkUESkookumScriptBehaviorComponent.hpp"
#include "SkUEActor.hpp"
#include "SkUEName.hpp"

#include <SkookumScript/SkBoolean.hpp>

//=======================================================================================
// Method Definitions
//=======================================================================================

namespace SkUESkookumScriptBehaviorComponent_Impl
  {

  //---------------------------------------------------------------------------------------
  // Default constructor sets USkookumScriptBehaviorComponent pointer to null
  void mthd_ctor(SkInvokedMethod * scope_p, SkInstance ** result_pp)
    {
    scope_p->get_this()->construct<SkUESkookumScriptBehaviorComponent>(nullptr);
    }

  //---------------------------------------------------------------------------------------
  // attach(Actor actor, Name component_name)
  void mthd_attach(SkInvokedMethod * scope_p, SkInstance ** result_pp)
    {
    SkInstance * this_p = scope_p->get_this();
    AActor * actor_p = scope_p->get_arg<SkUEActor>(SkArg_1);
    FName name = scope_p->get_arg<SkUEName>(SkArg_2);

    // If already attached, don't do anything
    bool remove_reference = false;
    USkookumScriptBehaviorComponent * old_component_p = this_p->as<SkUESkookumScriptBehaviorComponent>();
    SK_ASSERTX(!old_component_p, a_cstr_format("Trying to attach SkookumScriptBehaviorComponent to actor '%S' but componant is still attached to actor '%S'. Please deatch() first.", *actor_p->GetName(), *old_component_p->GetOwner()->GetName()));
    if (!old_component_p)
      {
      // Figure out which exact subclass of SkookumScriptBehaviorComponent we want to instantiate here
      UClass * ue_class_p;
      SkClass * sk_class_p = SkUEClassBindingHelper::find_most_derived_super_class_known_to_ue(this_p->get_class(), &ue_class_p);
      SK_ASSERTX(sk_class_p->is_class(*SkUESkookumScriptBehaviorComponent::get_class()), a_str_format("Trying to attach a component of class '%s' which is not properly derived from '%s'", this_p->get_class()->get_name_cstr_dbg(), SkUESkookumScriptBehaviorComponent::get_class()->get_name_cstr_dbg()));
      // Create a new UE4 ActorComponent and register it with the actor
      USkookumScriptBehaviorComponent * new_component_p = NewObject<USkookumScriptBehaviorComponent>(actor_p, ue_class_p, name);
      new_component_p->attach(this_p);
      }
    }

  //---------------------------------------------------------------------------------------
  // detach()
  void mthd_detach(SkInvokedMethod * scope_p, SkInstance ** result_pp)
    {
    SkInstance * this_p = scope_p->get_this();
    USkookumScriptBehaviorComponent * component_p = this_p->as<SkUESkookumScriptBehaviorComponent>();
    SK_ASSERTX(component_p && component_p->get_sk_component_instance() == this_p, "Trying to detach SkookumScriptBehaviorComponent but it is not attached to anything.");
    if (component_p)
      {
      component_p->detach();
      }
    }

  //---------------------------------------------------------------------------------------
  // attached?()
  void mthd_attached_Q(SkInvokedMethod * scope_p, SkInstance ** result_pp)
    {
    if (result_pp) // Do nothing if result not desired
      {
      USkookumScriptBehaviorComponent * component_p = scope_p->this_as<SkUESkookumScriptBehaviorComponent>();
      *result_pp = SkBoolean::new_instance(component_p != nullptr);
      }
    }

  //---------------------------------------------------------------------------------------

  static const SkClass::MethodInitializerFunc methods_i[] =
    {
      { "!",          mthd_ctor },
      { "attach",     mthd_attach },
      { "detach",     mthd_detach },
      { "attached?",  mthd_attached_Q },
    };

  } // SkUESkookumScriptBehaviorComponent_Impl

//---------------------------------------------------------------------------------------

void SkUESkookumScriptBehaviorComponent::register_bindings()
  {
  // The Entity base class provides all the basic functionality so here we just need to initialize our class pointers
  tBindingAbstract::initialize_class("SkookumScriptBehaviorComponent");

  ms_class_p->register_method_func_bulk(SkUESkookumScriptBehaviorComponent_Impl::methods_i, A_COUNT_OF(SkUESkookumScriptBehaviorComponent_Impl::methods_i), SkBindFlag_instance_no_rebind);

  SkUEClassBindingHelper::add_static_class_mapping(get_class(), ms_uclass_p);
  }

//---------------------------------------------------------------------------------------

SkClass * SkUESkookumScriptBehaviorComponent::get_class()
  {
  return ms_class_p;
  }

