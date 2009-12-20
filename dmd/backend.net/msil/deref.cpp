// $Id: deref.cpp 24625 2009-07-31 01:05:55Z unknown $
// Copyright (c) 2009 Cristian L. Vlasceanu

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
#include "backend.net/type.h"
#include "backend.net/msil/deref.h"

using namespace std;


bool isValueType(TYPE& t)
{
    //slices are implemented as ArraySegment's 
    return t.isSliceType()
#if VALUETYPE_STRUCT
        || t.isStructType()
#endif
        ;
}


elem* StoreIndirect::emitILAsm(wostream& out)
{
    indent(out);
#if !VALUETYPE_STRUCT
    if (blit_)
    {
        out << "call void [dnetlib]runtime.dnet::blit(object, object)";
    }
    else
#endif
        if (isValueType(t_))
    {
        out << "stobj " << t_.getSuffix();
    }
    else
    {
        out << "stind." << t_.getElemSuffix();
    }
    out << '\n';
    return this;
}


elem* LoadIndirect::emitILAsm(wostream& out)
{   
    indent(out);
    if (isValueType(t_))
    {
        out << "ldobj " << t_.name();
    }
    else
    {
        out  << "ldind." << t_.getElemSuffix();
    }
    out << '\n';
    return this;
}


elem* Dereference::emitILAsm(wostream& out) 
{
    //does not emit any code
    IF_DEBUG(indent(out) << "// deref '" << getName() << "'\n");
    return NULL;
}


elem* Dereference::load(block& blk, Loc* loc, bool)
{
    if (!loc) loc = &blk.loc();
    return blk.add(*new LoadIndirect(toILType(*loc, getType())));
}


elem* Dereference::store(block& blk, Loc* loc)
{
    assert(blk.back() == this);
    blk.pop_back(false);
    blk.swapTail();
    blk.add(*this);
    if (!loc) loc = &blk.loc();
    return blk.add(*new StoreIndirect(toILType(*loc, getType())));
}
