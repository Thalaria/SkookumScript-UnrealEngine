//=======================================================================================
// SkookumScript C++ library.
// Copyright (c) 2015 Agog Labs Inc. All rights reserved.
//
// Additional bindings for the Entity (= UObject) class 
//
// Author: Markus Breyer
//=======================================================================================

#pragma once

//=======================================================================================
// Includes
//=======================================================================================

#include <SkUEGeneratedBindings.generated.hpp>

//=======================================================================================
// Global Functions
//=======================================================================================

//---------------------------------------------------------------------------------------
// Bindings for the Entity (= UObject) class 
class SkUEEntity_Ext : public SkUEEntity
  {
  public:
    static void register_bindings();
  };

