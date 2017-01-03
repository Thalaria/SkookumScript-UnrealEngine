//=======================================================================================
// SkookumScript C++ library.
// Copyright (c) 2001 Agog Labs Inc.,
// All rights reserved.
//
// Instance of a class with data members
// Author(s):   Conan Reis
// Notes:          
//=======================================================================================

#pragma once

//=======================================================================================
// Includes
//=======================================================================================

#include <AgogCore/APSorted.hpp>
#include <SkookumScript/SkInstanceList.hpp>

//=======================================================================================
// Global Structures
//=======================================================================================


//---------------------------------------------------------------------------------------
// Notes      Class instance objects with one or more data members
// Subclasses SkDataInstance(SkActor), SkMetaClass
// Author(s)  Conan Reis
class SK_API SkDataInstance : public SkInstance
  {
  public:
  
  friend class AObjReusePool<SkDataInstance>;

  // Common Methods

    SK_NEW_OPERATORS(SkDataInstance);
    SkDataInstance(SkClass * class_p);
    virtual ~SkDataInstance();


  // Methods

    void          add_data_members();
    void          data_empty()          { m_data.empty(); }
    SkInstance *  get_data_by_idx(uint32_t data_idx) const;
    void          set_data_by_idx(uint32_t data_idx, SkInstance * obj_p);

    // Overriding from SkInstance

    virtual void          delete_this() override;
    virtual SkInstance *  get_data_by_name(const ASymbol & name) const override;
    virtual bool          set_data_by_name(const ASymbol & name, SkInstance * data_p) override;

  // Pool Allocation Methods

    static SkDataInstance *                 new_instance(SkClass * class_p);
    static AObjReusePool<SkDataInstance> &  get_pool();

  protected:

    friend class AObjReusePool<SkDataInstance>;

  // Internal Methods

    // Default constructor only may be called by pool_new()
    SkDataInstance();

    SkDataInstance ** get_pool_unused_next() { return (SkDataInstance **)&m_user_data.m_data.m_uintptr; } // Area in this class where to store the pointer to the next unused object when not in use

  // Data Members

    // Array of class instance data members - accessed by symbol name.
    // $Revisit - CReis It may be possible to rewrite this so that a direct index can be
    // used rather than a binary search of the symbols
    SkInstanceList m_data;

  #if (SKOOKUM & SK_DEBUG)
    // This magic number is stored in the user data to allow sanity checking code to verify we are dealing in fact with a data instance
    // It's not really a pointer, thus no _p postfix
    static void * const ms_magic_marker;

    // Called when a mismatch is detected - alerts the user and returns nil
    SkInstance * on_magic_marker_mismatch(uint32_t data_idx) const;
  #endif

  };  // SkDataInstance


//=======================================================================================
// Inline Methods
//=======================================================================================


#ifndef A_INL_IN_CPP
#include <SkookumScript/SkDataInstance.inl>
#endif

