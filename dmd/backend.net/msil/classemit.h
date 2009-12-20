#ifndef CLASS_H__49B94BA4_8B9B_48A1_AA00_1049426E8792
#define CLASS_H__49B94BA4_8B9B_48A1_AA00_1049426E8792
// -*- tab-width: 4; indent-tabs-mode: nil;  -*-
// vim: tabstop=4:softtabstop=4:expandtab:shiftwidth=4
//
// $Id: classemit.h 24625 2009-07-31 01:05:55Z unknown $
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
#include "backend.net/elem.h"
#include "backend.net/utils.h"
#include "backend.net/msil/aggr.h"


// All class attributes
#define CLASS_ATTRIBS \
    CLASS_ATTR(private) \
    CLASS_ATTR(public) \
    CLASS_ATTR(abstract) \
    CLASS_ATTR(interface) \
    CLASS_ATTR(sealed) \
    CLASS_ATTR(ansi) \
    CLASS_ATTR(autochar) \
    CLASS_ATTR(unicode) \
    CLASS_ATTR(auto) \
    CLASS_ATTR(explicit) \
    CLASS_ATTR(sequential)

#define CLASS_ATTR(x) c_##x,
enum ClassAttr
{
    CLASS_ATTRIBS
};
#undef CLASS_ATTR

struct ClassDeclaration;
struct CtorDeclaration;


///<summary>
/// Emits all the fields and methods for a class
///</summary>
class Class : public Aggregate
{
public:
    Class(ClassDeclaration&, unsigned depth);

    Class* isClass() { return this; }

    ClassDeclaration& getDecl()
    {
        return DEREF(Aggregate::getDecl().isClassDeclaration());
    }

private:
    virtual elem* emitILAsm(std::wostream&);

    ///Implements the behavior described in section
    ///"Derived Class Member Function Hijacking #2"
    /// of Walter Bright's article:
    ///http://www.digitalmars.com/d/2.0/hijack.html
    void checkHiddenFunctions();

    /***** helpers called from emitILAsm() *****/
    std::wostream& emitAttr(std::wostream&);
    std::wostream& emitBases(std::wostream&);
    std::wostream& emitHeader(std::wostream&);

    void emitBase(const char* func, std::wostream&, unsigned);

    //ClassAttr visibility_;
    //ClassAttr marshalling_;
    //ClassAttr layout_;
};


Aggregate& getAggregate(AggregateDeclaration&);

///<summary>
///return the Class object associated with the declaration
///</summary>
Class& getClass(ClassDeclaration&);


#endif // CLASS_H__49B94BA4_8B9B_48A1_AA00_1049426E8792
