#ifndef UTILS_H__4800B6FE_3C3B_4FA3_9C57_B5A5C046E462
#define UTILS_H__4800B6FE_3C3B_4FA3_9C57_B5A5C046E462
// -*- tab-width: 4; indent-tabs-mode: nil;  -*-
// vim: tabstop=4:softtabstop=4:expandtab:shiftwidth=4
//
// $Id: utils.h 257 2009-03-17 01:19:54Z cristiv $
//
// Copyright (c) 2008, 2009 Cristian L. Vlasceanu

/*****
This license governs use of the accompanying software. If you use the software, you
accept this license. If you do not accept the license, do not use the software.

1. Definitions
The terms "reproduce," "reproduction," "derivative works," and "distribution" have the
same meaning here as under U.S. copyright law.
A "contribution" is the original software, or any additions or changes to the software.
A "contributor" is any person that distributes its contribution under this license.
"Licensed patents" are a contributor's patent claims that read directly on its contribution.

2. Grant of Rights
(A) Copyright Grant- Subject to the terms of this license, including the license conditions and limitations in section 3,
each contributor grants you a non-exclusive, worldwide, royalty-free copyright license to reproduce its contribution, 
prepare derivative works of its contribution, and distribute its contribution or any derivative works that you create.
(B) Patent Grant- Subject to the terms of this license, including the license conditions and limitations in section 3, 
each contributor grants you a non-exclusive, worldwide, royalty-free license under its licensed patents to make, have made,
use, sell, offer for sale, import, and/or otherwise dispose of its contribution in the software or derivative works of the 
contribution in the software.

3. Conditions and Limitations
(A) No Trademark License- This license does not grant you rights to use any contributors' name, logo, or trademarks.
(B) If you bring a patent claim against any contributor over patents that you claim are infringed by the software,
your patent license from such contributor to the software ends automatically.
(C) If you distribute any portion of the software, you must retain all copyright, patent, trademark,
and attribution notices that are present in the software.
(D) If you distribute any portion of the software in source code form, you may do so only under this license by 
including a complete copy of this license with your distribution. If you distribute any portion of the software 
in compiled or object code form, you may only do so under a license that complies with this license.
(E) The software is licensed "as-is." You bear the risk of using it. The contributors give no express warranties,
guarantees or conditions. You may have additional consumer rights under your local laws which this license cannot change.
To the extent permitted under your local laws, the contributors exclude the implied warranties of merchantability,
fitness for a particular purpose and non-infringement.
*****/
#ifdef _DEBUG
 #define DEBUG
#endif
#include <cassert>
#include <string>

#ifdef DEBUG
#include <iostream>
#define IF_DEBUG(x) x
 #if _WIN32
  #define IF_WIN_DEBUG(x) x
//including <winbase.h> conflicts with stuff in the front-end
  extern "C" void __declspec(dllimport) __stdcall DebugBreak();
  extern "C" void __declspec(dllimport) __stdcall OutputDebugStringA(const char*);
 #else
  #define IF_WIN_DEBUG(x)
 #endif
#else
#define IF_DEBUG(x)
#define IF_WIN_DEBUG(x)
#endif
 
#ifdef __GNUC__
 #define __FUNCSIG__    __PRETTY_FUNCTION__
 #define __FUNCTION__   __func__ 
#endif

#if !defined(DEBUG)
#define THROW_NOT_IMPL
#else
#define THROW_NOT_IMPL std::cout << "NOT IMPLEMENTED: " << __FUNCSIG__ << std::endl
#endif //DEBUG

#define NOT_IMPLEMENTED(...) THROW_NOT_IMPL; return __VA_ARGS__

#define DEREF(ptr)  (assert(ptr), *ptr)

#define STRINGIZE_HELPER(a) #a
#define STRINGIZE(a) STRINGIZE_HELPER(a)


template<typename T>
class Temporary
{
    T val_;
    T* ref_;

    Temporary(const Temporary&);
    Temporary& operator=(const Temporary&);

public:
    Temporary() : ref_(NULL)
    {
    }
    explicit Temporary(T& ref) : val_(ref), ref_(&ref)
    { 
    }
    Temporary(T& ref, T newVal) : val_(ref), ref_(&ref)
    { 
        ref = newVal;
    }
    ~Temporary()
    {
        if (ref_)
        {
            *ref_ = val_; // restore original value
        }
    }
    void swap(Temporary& other) throw()
    {
        std::swap(val_, other.val_);
        std::swap(ref_, other.ref_);
    }
    Temporary& operator=(T& ref)
    {
        assert(!ref_);
        Temporary tmp(ref);
        tmp.swap(*this);
        assert(ref_ == &ref);
        return *this;
    }
};


template<typename T>
inline void replaceBackSlashes(T& path)
{
    for (size_t i = 0; i != path.size(); ++i)
    {
        if (path[i] == '\\')
        {
            path[i] = '/';
        }
    }
}


#define WARNING(loc, fmt, ...)                              \
    if (global.params.warnings) {                           \
        fprintf(stdmsg, "warning - ");                      \
        error((loc), (fmt), ##__VA_ARGS__);                 \
    } else {                                                \
        warning("%s: " fmt, loc.toChars(), ##__VA_ARGS__);  \
    }


#endif // UTILS_H__4800B6FE_3C3B_4FA3_9C57_B5A5C046E462
