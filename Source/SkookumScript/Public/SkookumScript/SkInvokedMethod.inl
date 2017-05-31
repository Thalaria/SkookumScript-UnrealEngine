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
// SkookumScript C++ library.
//
// Class wrapper for executed/called/invoked methods inline file
//=======================================================================================


//=======================================================================================
// Includes
//=======================================================================================

#include <AgogCore/AObjReusePool.hpp>
#include <SkookumScript/SkInstance.hpp>
#include <SkookumScript/SkMethod.hpp>


//=======================================================================================
// SkInvokedMethod Inline Methods
//=======================================================================================

//=======================================================================================
// SkInvokedDeferrableMethod Inline Methods
//=======================================================================================

//---------------------------------------------------------------------------------------
// Constructor
A_INLINE SkInvokedMethod::SkInvokedMethod(SkInvokedBase * caller_p, SkObjectBase * scope_p, SkMethodBase * method_p)
  : SkInvokedContextBase(caller_p, scope_p), m_method_p(method_p)
  {
  m_data.ensure_size(method_p->get_invoked_data_array_size());
  }

//---------------------------------------------------------------------------------------
// Constructor
A_INLINE SkInvokedMethod::SkInvokedMethod(SkInvokedBase * caller_p, SkObjectBase * scope_p, SkMethodBase * method_p, SkInstance ** data_mem_pp)
  : SkInvokedContextBase(caller_p, scope_p), m_method_p(method_p)
  {
  m_data.set_custom_memory_empty_unsafe(data_mem_pp, method_p->get_invoked_data_array_size());
  }

//---------------------------------------------------------------------------------------
// Destructor
A_INLINE SkInvokedMethod::~SkInvokedMethod()
  {
  data_empty();
  m_data.set_custom_memory_empty_unsafe(nullptr, 0); // Remove custom memory so the array will not try to free it
  m_calls.empty();       // $Revisit - Empty call list - might be redundant
  //AListNode<SkInvokedBase>::remove(); // $Revisit - MBreyer needed?
  }

//---------------------------------------------------------------------------------------
// Calls the wrapped method expecting immediate completion with an optional
//             result.
// Arg         result_pp - pointer to a pointer to store the instance resulting from the
//             invocation of this expression.  If it is nullptr, then the result does not
//             need to be returned and only side-effects are desired.  (Default nullptr)
// Author(s):   Conan Reis
A_INLINE void SkInvokedDeferrableMethod::invoke(
  SkInstance ** result_pp // = nullptr
  )
  {
  m_method_p->invoke(this, this, result_pp);
  }

//---------------------------------------------------------------------------------------
// Calls the method essentially as a simple coroutine - with the possibility 
//             that completion will be deferred - i.e. it will be durational.
// Returns:    deferred invoked sub-expression or nullptr if completed immediately.
// Arg         result_pp - pointer to a pointer to store the instance resulting from the
//             invocation of this expression.  The result will probably only be interesting
//             if it completes immediately.  If it is nullptr, then the result does not need
//             to be returned and only side-effects are desired.  (Default nullptr)
// Author(s):   Conan Reis
A_INLINE SkInvokedBase * SkInvokedDeferrableMethod::invoke_deferred(
  SkInstance ** result_pp // = nullptr
  )
  {
  SkInvokedBase * deferred_p = static_cast<SkMethod *>(m_method_p)->invoke_deferred(this, this, result_pp);

  if (deferred_p)
    {
    pending_deferred(deferred_p);
    }

  return deferred_p;
  }

//---------------------------------------------------------------------------------------
// Calls the wrapped method expecting immediate completion and returns the
//             result.
// Returns:    Result from invocation.
// Author(s):   Conan Reis
A_INLINE SkInstance * SkInvokedDeferrableMethod::invoke_now()
  {
  SkInstance * result_p;

  m_method_p->invoke(this, this, &result_p);

  return result_p;
  }


//=======================================================================================
// SkIExternalMethodCallWrapper Inline Methods
//=======================================================================================

//---------------------------------------------------------------------------------------
// Author(s):   Conan Reis, Richard Orth
A_INLINE SkIExternalMethodCallWrapper::~SkIExternalMethodCallWrapper()
  {
  if (m_finished_p)
    {
    *m_finished_p = true;
    }

  // want to keep the external code around
  ((SkMethod*)m_method_p)->replace_expression(nullptr);

  delete m_method_p;
  }

//---------------------------------------------------------------------------------------
// If the external caller no longer exists then we need to clear the finish pointer
//             so don't stomp memory
// Author(s):   Richard Orth
A_INLINE void SkIExternalMethodCallWrapper::clear_finished_check()
  {
  m_finished_p = nullptr;
  }
