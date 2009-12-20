// -*- tab-width: 4; indent-tabs-mode: nil;  -*-
// vim: tabstop=4:softtabstop=4:expandtab:shiftwidth=4
//
// $Id: const.cpp 24625 2009-07-31 01:05:55Z unknown $
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
#include <sstream>
#include "declaration.h"
#include "backend.net/type.h"
#include "backend.net/msil/const.h"

using namespace std;


StringLiteral::StringLiteral(block& blk, const string& str, Encoding enc)
    : AutoManaged<Sequence>(blk)
    , str_(str)
    , ptr_(NULL)
    , size_(str.size())
    , encoding_(enc)
{
    ptr_ = &str_[0];
}


StringLiteral::StringLiteral(StringExp& exp, block& blk)
    : AutoManaged<Sequence>(blk)
    , ptr_(exp.string)
    , size_(exp.len)
    , encoding_(e_UTF8)
{
    switch (exp.sz)
    {
    case 1: /* encoding_ = e_UTF8; */ 
        break;
    case 2: encoding_ = e_UTF16;
        break;
    case 4: encoding_ = e_UTF32;
        error(exp.loc, "UTF32 not supported in this version of %s", BackEnd::name());
        break;

    default:
        error(exp.loc, "unexpected char size in string literal");
    }
}


wstring StringLiteral::toString() const
{
    wstring str(size_, ' '); //returned string

    switch (encoding_)
    {
    case e_SystemString:
    case e_UTF8:
        size_ = mbstowcs(&str[0], reinterpret_cast<const char*>(ptr_), size_);
        break;

    case e_UTF16:
        {
            const wchar_t* begin = reinterpret_cast<const wchar_t*>(ptr_);
            const wchar_t* end = begin + size_;
            std::copy(begin, end, &str[0]);
        }
        break;

    default:
        //UTF32 not implemented
        assert(false);
    }

    for (size_t i = 0; i != str.size(); )
    {
        wchar_t c = str[i];
        if (iswprint(c) && c != L'"' && c != L'\\')
        {
            ++i;
        }
        else
        {
            //escape unprintable chars
            //todo: what about ldstr bytearray ( ... )?
            str[i++] = '\\';
            wostringstream buf;
            buf << '0' << oct << (c & 0xff) /* << "\\0" << ((c >> 8) & 0xff) */;
            str.insert(i, buf.str());
            i += buf.str().size();
        }
    }
    return str;
}


elem* StringLiteral::emitILAsm(wostream& out)
{
    switch (encoding_)
    {
    case e_UTF8:
        indent(out) << "call class [mscorlib]System.Text.Encoding "
            "[mscorlib]System.Text.Encoding::get_UTF8()\n";
        break;
    }

    indent(out) << IL_ldstr << " \"";
    if (str_.empty())
    {
        out << toString();
    }
    else
    {
        out << str_.c_str();
    }
    out << "\"\n";

    switch (encoding_)
    {
    case e_UTF8:
        indent(out) << "callvirt instance uint8[] "
            "[mscorlib]System.Text.Encoding::GetBytes(string)\n";
        break;

    case e_UTF16:
        indent(out) << "call instance char[] string::ToCharArray()\n";
        break;
    }
    return this;
}


int32_t StringLiteral::getMaxStackDelta() const 
{
    switch (encoding_)
    {
    case e_UTF8:
        return 2; // ldstr + Encoding::get_UTF8

    default:
        return 1; // same as getStackDelta()
    }
}
