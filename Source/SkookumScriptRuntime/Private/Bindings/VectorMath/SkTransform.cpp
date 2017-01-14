//=======================================================================================
// SkookumScript C++ library.
// Copyright (c) 2015 Agog Labs Inc. All rights reserved.
//
// SkookumScript transform (position + rotation + scale) class
//
// Author: Markus Breyer
//=======================================================================================

//=======================================================================================
// Includes
//=======================================================================================

#include "../../SkookumScriptRuntimePrivatePCH.h"

#include "SkTransform.hpp"
#include "SkVector3.hpp"
#include "SkRotation.hpp"

#include "UObject/Package.h"

//=======================================================================================
// Method Definitions
//=======================================================================================

namespace SkTransform_Impl
  {

  //---------------------------------------------------------------------------------------
  // # Skookum:   Transform@String() String
  // # Author(s): Markus Breyer
  static void mthd_String(SkInvokedMethod * scope_p, SkInstance ** result_pp)
    {
    // Do nothing if result not desired
    if (result_pp)
      {
      const FTransform &  transform = scope_p->this_as<SkTransform>();
      const FVector &     location = transform.GetLocation();
      const FRotator      rotator(transform.GetRotation());
      const FVector &     scale = transform.GetScale3D();
      AString             str(128u, "t=(%g, %g, %g) yaw=%g pitch=%g roll=%g s=(%g, %g, %g)", 
                            double(location.X), double(location.Y), double(location.Z),
                            double(rotator.Yaw), double(rotator.Pitch), double(rotator.Roll),
                            double(scale.X), double(scale.Y), double(scale.Z));

      *result_pp = SkString::new_instance(str);
      }
    }

  //---------------------------------------------------------------------------------------
  // # Skookum:   Transform@identity() Transform
  // # Author(s): Markus Breyer
  static void mthd_identity(SkInvokedMethod * scope_p, SkInstance ** result_pp)
    {
    SkInstance * this_p = scope_p->get_this();

    this_p->as<SkTransform>().SetIdentity();

    // Return this if result desired
    if (result_pp)
      {
      this_p->reference();
      *result_pp = this_p;
      }
    }

  //---------------------------------------------------------------------------------------

  // Instance method array
  static const SkClass::MethodInitializerFunc methods_i[] =
    {
      { "String",       mthd_String },
      { "identity",     mthd_identity },
    };

  } // namespace

//---------------------------------------------------------------------------------------

void SkTransform::register_bindings()
  {
  tBindingBase::register_bindings("Transform");

  ms_class_p->register_method_func_bulk(SkTransform_Impl::methods_i, A_COUNT_OF(SkTransform_Impl::methods_i), SkBindFlag_instance_no_rebind);

  ms_class_p->register_raw_accessor_func(&SkUEClassBindingHelper::access_raw_data_struct<SkTransform>);

  // Handle special case here - in UE4, the scale variable is called "Scale3D" while in Sk, we decided to call it just "scale"
  UStruct * ue_struct_p = FindObjectChecked<UScriptStruct>(UObject::StaticClass()->GetOutermost(), TEXT("Transform"), false);
  UProperty * ue_scale_var_p = FindObjectChecked<UProperty>(ue_struct_p, TEXT("Scale3D"), false);
  ms_class_p->resolve_raw_data("@scale", SkUEClassBindingHelper::compute_raw_data_info(ue_scale_var_p));
  SkUEClassBindingHelper::resolve_raw_data(ms_class_p, ue_struct_p); // Resolve the remaining raw data members as usual
  }

//---------------------------------------------------------------------------------------

SkClass * SkTransform::get_class()
  {
  return ms_class_p;
  }
