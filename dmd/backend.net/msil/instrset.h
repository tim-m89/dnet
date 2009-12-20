#ifndef UNKNOWN_STACK_DELTA
 #define UNKNOWN_STACK_DELTA INT_MIN
#endif
// -*- tab-width: 4; indent-tabs-mode: nil;  -*-
// vim: tabstop=4:softtabstop=4:expandtab:shiftwidth=4
//
// $Id: instrset.h 24625 2009-07-31 01:05:55Z unknown $
//
// Copyright (c) 2008 Cristian L. Vlasceanu
// Copyright (c) 2008 Ionut-Gabriel Burete

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
//
// extend this list as needed:
// 
IL_INST(add, -1)
IL_INST(and, -1)
IL_INST(beq, -2)
IL_INST(bge, -2)
IL_INST(bgt, -2)
IL_INST(ble, -2)
IL_INST(blt, -2)
IL_INST(bne_un, -2)
IL_INST(box, 0)
IL_INST(br, 0)
IL_INST(brfalse, -1)
IL_INST(brnull, -1)
IL_INST(brtrue, -1)
IL_INST(call, UNKNOWN_STACK_DELTA)
IL_INST(callvirt, UNKNOWN_STACK_DELTA)
IL_INST(ceq, -1)
IL_INST(cgt, -1)
IL_INST(cgt_un, -1)
IL_INST(clt, -1)
IL_INST(div, -1)
IL_INST(dup, 1)
IL_INST(isinst, 0)
IL_INST(ldelem_ref, -1)
IL_INST(ldlen, 0)
IL_INST(ldftn, 1)
IL_INST(ldstr, 1)
IL_INST(ldvirtftn, 1)
IL_INST(leave, 0)
IL_INST(mul, -1)
IL_INST(newobj, UNKNOWN_STACK_DELTA)
IL_INST(neg, 0)
IL_INST(not, 0)
IL_INST(nop, 0)
IL_INST(or, -1)
IL_INST(pop, -1)
IL_INST(rem, -1)
IL_INST(rem_un, -1)
IL_INST(ret, UNKNOWN_STACK_DELTA)
IL_INST(stelem_ref, -3)
IL_INST(shl, -1)
IL_INST(shr, -1)
IL_INST(shr_un, -1)
IL_INST(sub, -1)
IL_INST(switch, -1)
IL_INST(throw, -1)
IL_INST(unbox, 0)
IL_INST(unbox_any, 0)
IL_INST(xor, -1)
