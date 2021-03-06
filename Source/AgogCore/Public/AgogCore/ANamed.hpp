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
// Named object class declaration file
//=======================================================================================

#pragma once

//=======================================================================================
// Includes
//=======================================================================================

#include <AgogCore/ASymbol.hpp>


//=======================================================================================
// Global Structures
//=======================================================================================

//---------------------------------------------------------------------------------------
// Notes      Named object class
// See Also   ASymbol
// Author(s)  Conan Reis
class A_API ANamed
  {
  public:

    // Common Methods

    explicit ANamed(const ASymbol & name = ASymbol::get_null());
    ANamed(const ANamed & source);
    ANamed & operator=(const ANamed & source);

    // Converter Methods

    operator const ASymbol & () const;

    // Comparison Methods - used for sorting esp. in arrays like APSorted

    bool operator==(const ANamed & named) const;
    bool operator<(const ANamed & named) const;

    // Accessor Methods

    ASymbol &       get_name()                      { return m_name; }
    const ASymbol & get_name() const                { return m_name; }
    uint32_t        get_name_id() const             { return m_name.get_id(); }
    void            set_name(const ASymbol & name);

    #if defined(A_SYMBOL_STR_DB)
      AString       get_name_str() const;
      const char *  get_name_cstr() const;
    #endif

    AString         get_name_str_dbg() const;
    const char *    get_name_cstr_dbg() const;

  protected:

    // Data Members

    ASymbol m_name;

  };  // ANamed


//=======================================================================================
// Inline Methods
//=======================================================================================

#ifndef A_INL_IN_CPP
  #include <AgogCore/ANamed.inl>
#endif
