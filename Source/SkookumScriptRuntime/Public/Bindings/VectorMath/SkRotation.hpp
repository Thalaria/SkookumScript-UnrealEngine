//=======================================================================================
// SkookumScript C++ library.
// Copyright (c) 2015 Agog Labs Inc. All rights reserved.
//
// SkookumScript rotation/quaternion class
//
// Author: Markus Breyer
//=======================================================================================

#pragma once

//=======================================================================================
// Includes
//=======================================================================================

#include "../SkUEClassBinding.hpp"
#include "Math/UnrealMath.h" // Vector math functions

//---------------------------------------------------------------------------------------
// SkookumScript rotation/quaternion class
class SKOOKUMSCRIPTRUNTIME_API SkRotation : public SkClassBindingSimpleForceInit<SkRotation, FQuat>
  {
  public:

    static void       register_bindings();
    static SkClass *  get_class();

  };
