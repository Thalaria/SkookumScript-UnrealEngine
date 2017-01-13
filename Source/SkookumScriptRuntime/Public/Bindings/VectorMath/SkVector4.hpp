//=======================================================================================
// SkookumScript C++ library.
// Copyright (c) 2015 Agog Labs Inc. All rights reserved.
//
// SkookumScript 4D vector class
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
// SkookumScript 4D vector class
class SKOOKUMSCRIPTRUNTIME_API SkVector4 : public SkClassBindingSimpleForceInit<SkVector4, FVector4>
  {
  public:

    static void       register_bindings();
    static SkClass *  get_class();

  };
