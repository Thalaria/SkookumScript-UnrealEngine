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
// Agog Labs C++ library.
//
// Simple string based exception class definition module
//=======================================================================================


//=======================================================================================
// Includes
//=======================================================================================

#include <AgogCore/AgogCore.hpp> // Always include AgogCore first (as some builds require a designated precompiled header)
#include <AgogCore/AException.hpp>


//=======================================================================================
// Method Definitions
//=======================================================================================

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Common Methods
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

//---------------------------------------------------------------------------------------
//  AString constructor for simple error which displays a given message and
//              the file name and line where the exception is thrown.  If an exception
//              level is not given, it defaults to an informational exception.
// Returns:     itself
// Arg          err_id - Numeric id for programmatic identification of the specific
//              exception - usually from an enumerated type or the address of the
//              excepting function
// Arg          action - Suggested course of action to resolve this error.  See available
//              choices and there descriptions at eAErrAction.
// Examples:    throw AException(ERR_overflow);
// See:         AEx<>, eAErrAction, AErrorOutputBase, ADebug
// Author(s):    Conan Reis
AException::AException(
  uint32_t    err_id,
  eAErrAction action // = AErrAction_continue
  ) :
  m_err_id(err_id),
  m_action(action)
  {
  //BREAK();  // This should be more useful than annoying
  }


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Modifying Methods
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

//---------------------------------------------------------------------------------------
//  Deals with the exception and determines what action to take - this has
//              usually already been determined by the current Error output object.
// Returns:     Type of action to take
// Modifiers:    virtual
// Author(s):    Conan Reis
eAErrAction AException::resolve()
  {
  return m_action;
  }

