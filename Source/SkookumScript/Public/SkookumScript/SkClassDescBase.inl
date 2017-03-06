
// SkookumScript C++ library.
// Copyright (c) 2001 Agog Labs Inc.,
// All rights reserved.
//
// Data structures for class descriptors and class objects
// Author(s):   Conan Reis
// Notes:          
//=======================================================================================


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
// Includes
//=======================================================================================


//=======================================================================================
// SkClassDescBase Inline Methods
//=======================================================================================

//---------------------------------------------------------------------------------------
// Increments the reference/usage count of this class [if such a mechanism is
//             used for this class type].
// Modifiers:   virtual
// Author(s):   Conan Reis
A_INLINE void SkClassDescBase::reference() const
  {
  // By default, do nothing.
  }

//---------------------------------------------------------------------------------------
// Decrements the reference/usage count of this class [if such a mechanism is
//             used for this class type].
// Modifiers:   virtual
// Author(s):   Conan Reis
A_INLINE void SkClassDescBase::dereference()
  {
  // By default, do nothing.
  }

//---------------------------------------------------------------------------------------
// Same as dereference() in that it decrements the reference count to this
//             object, but it does not call on_no_references() if the reference count
//             becomes 0.
// Modifiers:   virtual
// Author(s):   Conan Reis
A_INLINE void SkClassDescBase::dereference_delay() const
  {
  // By default, do nothing.
  }

//---------------------------------------------------------------------------------------
// Returns true if it is an `Actor` class or a subclass of `Actor` (SkActorClass) or
// false if not.
// 
// Returns:   true if it is an actor class instance (SkActorClass)
// Notes:     Same as calling is_class(*SkBrain::ms_actor_class_p), but faster.
// Modifiers: virtual
// Author(s): Conan Reis
A_INLINE bool SkClassDescBase::is_builtin_actor_class() const
  {
  return false;
  }

//---------------------------------------------------------------------------------------
//  Determines if this type is a generic/reflective class.
//  [Generic classes are: ThisClass_ and ItemClass_.  The Auto_ class is replaced during
//  parse as its type is determined via its surrounding context.]
//
// #Examples
//   "ThisClass_" with "String" as a scope type becomes "String"
//
// #Modifiers virtual
// #See Also  as_finalized_generic()
// #Author(s) Conan Reis
A_INLINE bool SkClassDescBase::is_generic() const
  {
  return false;
  }

//---------------------------------------------------------------------------------------
// Returns true if it is a metaclass - i.e. using class scope.
// Returns:    true if it is a metaclass
// Modifiers:   virtual
// Author(s):   Conan Reis
A_INLINE bool SkClassDescBase::is_metaclass() const
  {
  return false;
  }


