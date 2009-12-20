#ifndef CONST_H__EF6F3C39_B10D_43EE_A0DE_03028D338E0B
#define CONST_H__EF6F3C39_B10D_43EE_A0DE_03028D338E0B
// -*- tab-width: 4; indent-tabs-mode: nil;  -*-
// vim: tabstop=4:softtabstop=4:expandtab:shiftwidth=4
//
// $Id: const.h 24625 2009-07-31 01:05:55Z unknown $
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
#include <math.h>
#include <iomanip>
#include <limits>
#include <string>
#include "backend.net/backend.h"
#include "backend.net/block.h"
#include "backend.net/elem.h"
#include "backend.net/type.h"
#include "backend.net/msil/instruction.h"

struct StringExp;


template<typename T>
inline bool _isnan(T x)
{
    return false;
}


template<typename T>
class Const : public AutoManaged<elem>
{
    TYPE* type_;
    T value_;

    Const(TYPE& type, T value, block& blk, Loc& loc)
        : AutoManaged<elem>(blk)
        , type_(type.getConstType())
        , value_(value)
    {
        if (!type_)
        {
            std::string err("cannot emit const of type ");
            err += type.name();
            BackEnd::fatalError(loc, err.c_str());
        }
#if 0
        if (type_ != &type)
        {
            Conversion::create(blk, type);
        }
#endif
    }

    elem* emitILAsm(std::wostream& out)
    {
        const unsigned p = std::numeric_limits<T>::digits10;
        indent(out) << "ldc." << type_->getSuffix() << " " << std::setprecision(p);
        if (isnan(value_))
        {
            out << "0";
        }
        else
        {
            out << value_;
        }
        out << '\n';
        return this;
    }

    int32_t getStackDelta() const { return 1; }

    bool isNull() const 
    { 
        return std::numeric_limits<T>::is_integer && !value_;
    }
    bool isConst() const { return true; }

public:
    static Const* create(TYPE& type, T value, block& block, Loc& loc)
    {
        return new Const(type, value, block, loc);
    }
};


class StringLiteral : public AutoManaged<Sequence>
{
    std::string str_;
    const void* ptr_;
    mutable size_t size_;
    Encoding encoding_;

    elem* emitILAsm(std::wostream& out);
    static std::wstring extract(const StringExp&, Encoding&);
    std::wstring toString() const;

public:
    StringLiteral(block&, const std::string&, Encoding = e_UTF8);
    StringLiteral(StringExp&, block&);

    int32_t getStackDelta() const { return 1; }
    int32_t getMaxStackDelta() const;

    StringLiteral* isStringLiteral() { return this; }

    void setEncoding(Encoding enc) { encoding_ = enc; }
    
    /*********** Sequence interface ************/
    size_t length() const { return size_; }
    TYPE* elemTYPE() const { return TYPE::Char; }
};


///<summary>
/// Helper that calls [mscorlib]System.String::Concat with 2 string arguments
///</summary>
class StringConcat : public elem
{
    elem* emitILAsm(std::wostream& out)
    {
        indent(out) << "call string [mscorlib]System.String::Concat(string, string)\n";
        return this;
    }

    int32_t getStackDelta() const { return -1; } // pops 2, pushes 1
};

#endif // CONST_H__EF6F3C39_B10D_43EE_A0DE_03028D338E0B
