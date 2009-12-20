
// Compiler implementation of the D programming language
// Copyright (c) 1999-2008 by Digital Mars
// All Rights Reserved
// written by Walter Bright
// http://www.digitalmars.com
// License for redistribution is by either the Artistic License
// in artistic.txt, or the GNU General Public License in gnu.txt.
// See the included readme.txt for details.

// Program to generate string files in d data structures.
// Saves much tedious typing, and eliminates typo problems.
// Generates:
//	id.h
//	id.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

struct Msgtable
{
	const char *ident;	// name to use in DMD source
	const char *name;	// name in D executable
};

Msgtable msgtable[] =
{
    { "IUnknown" },
    { "Object" },
    { "object" },
    { "max" },
    { "min" },
    { "This", "this" },
    { "ctor", "__ctor" },
    { "dtor", "__dtor" },
    { "cpctor", "__cpctor" },
    { "_postblit", "__postblit" },
    { "classInvariant", "__invariant" },
    { "unitTest", "__unitTest" },
    { "require", "__require" },
    { "ensure", "__ensure" },
    { "init" },
    { "size" },
    { "__sizeof", "sizeof" },
    { "alignof" },
    { "mangleof" },
    { "stringof" },
    { "tupleof" },
    { "length" },
    { "remove" },
    { "ptr" },
    { "funcptr" },
    { "dollar", "__dollar" },
    { "offset" },
    { "offsetof" },
    { "ModuleInfo" },
    { "ClassInfo" },
    { "classinfo" },
    { "typeinfo" },
    { "outer" },
    { "Exception" },
    { "AssociativeArray" },
    { "Throwable" },
    { "withSym", "__withSym" },
    { "result", "__result" },
    { "returnLabel", "__returnLabel" },
    { "delegate" },
    { "line" },
    { "empty", "" },
    { "p" },
    { "coverage", "__coverage" },
    { "__vptr" },
    { "__monitor" },
    { "system" },

    { "TypeInfo" },
    { "TypeInfo_Class" },
    { "TypeInfo_Interface" },
    { "TypeInfo_Struct" },
    { "TypeInfo_Enum" },
    { "TypeInfo_Typedef" },
    { "TypeInfo_Pointer" },
    { "TypeInfo_Array" },
    { "TypeInfo_StaticArray" },
    { "TypeInfo_AssociativeArray" },
    { "TypeInfo_Function" },
    { "TypeInfo_Delegate" },
    { "TypeInfo_Tuple" },
    { "TypeInfo_Const" },
    { "TypeInfo_Invariant" },
    { "TypeInfo_Shared" },
    { "elements" },
    { "_arguments_typeinfo" },
    { "_arguments" },
    { "_argptr" },
    { "_match" },
    { "destroy" },
    { "postblit" },

    { "LINE", "__LINE__" },
    { "FILE", "__FILE__" },
    { "DATE", "__DATE__" },
    { "TIME", "__TIME__" },
    { "TIMESTAMP", "__TIMESTAMP__" },
    { "VENDOR", "__VENDOR__" },
    { "VERSIONX", "__VERSION__" },
    { "EOFX", "__EOF__" },

    { "nan" },
    { "infinity" },
    { "dig" },
    { "epsilon" },
    { "mant_dig" },
    { "max_10_exp" },
    { "max_exp" },
    { "min_10_exp" },
    { "min_exp" },
    { "min_normal" },
    { "re" },
    { "im" },

    { "C" },
    { "D" },
    { "Windows" },
    { "Pascal" },
    { "System" },

    { "exit" },
    { "success" },
    { "failure" },

    { "keys" },
    { "values" },
    { "rehash" },

    { "sort" },
    { "reverse" },
    { "dup" },
    { "idup" },

    { "property" },

    // For inline assembler
    { "___out", "out" },
    { "___in", "in" },
    { "__int", "int" },
    { "__dollar", "$" },
    { "__LOCAL_SIZE" },

    // For operator overloads
    { "uadd",	 "opPos" },
    { "neg",     "opNeg" },
    { "com",     "opCom" },
    { "add",     "opAdd" },
    { "add_r",   "opAdd_r" },
    { "sub",     "opSub" },
    { "sub_r",   "opSub_r" },
    { "mul",     "opMul" },
    { "mul_r",   "opMul_r" },
    { "div",     "opDiv" },
    { "div_r",   "opDiv_r" },
    { "mod",     "opMod" },
    { "mod_r",   "opMod_r" },
    { "eq",      "opEquals" },
    { "cmp",     "opCmp" },
    { "iand",    "opAnd" },
    { "iand_r",  "opAnd_r" },
    { "ior",     "opOr" },
    { "ior_r",   "opOr_r" },
    { "ixor",    "opXor" },
    { "ixor_r",  "opXor_r" },
    { "shl",     "opShl" },
    { "shl_r",   "opShl_r" },
    { "shr",     "opShr" },
    { "shr_r",   "opShr_r" },
    { "ushr",    "opUShr" },
    { "ushr_r",  "opUShr_r" },
    { "cat",     "opCat" },
    { "cat_r",   "opCat_r" },
    { "assign",  "opAssign" },
    { "addass",  "opAddAssign" },
    { "subass",  "opSubAssign" },
    { "mulass",  "opMulAssign" },
    { "divass",  "opDivAssign" },
    { "modass",  "opModAssign" },
    { "andass",  "opAndAssign" },
    { "orass",   "opOrAssign" },
    { "xorass",  "opXorAssign" },
    { "shlass",  "opShlAssign" },
    { "shrass",  "opShrAssign" },
    { "ushrass", "opUShrAssign" },
    { "catass",  "opCatAssign" },
    { "postinc", "opPostInc" },
    { "postdec", "opPostDec" },
    { "index",	 "opIndex" },
    { "indexass", "opIndexAssign" },
    { "slice",	 "opSlice" },
    { "sliceass", "opSliceAssign" },
    { "call",	 "opCall" },
    { "cast",	 "opCast" },
    { "match",	 "opMatch" },
    { "next",	 "opNext" },
    { "opIn" },
    { "opIn_r" },
    { "opStar" },
    { "opDot" },
    { "opImplicitCast" },

    { "classNew", "new" },
    { "classDelete", "delete" },

    // For foreach
    { "apply", "opApply" },
    { "applyReverse", "opApplyReverse" },

#if 1
    { "Fempty", "empty" },
    { "Fhead", "front" },
    { "Ftoe", "back" },
    { "Fnext", "popFront" },
    { "Fretreat", "popBack" },
#else
    { "Fempty", "empty" },
    { "Fhead", "head" },
    { "Ftoe", "toe" },
    { "Fnext", "next" },
    { "Fretreat", "retreat" },
#endif

    { "adDup", "_adDupT" },
    { "adReverse", "_adReverse" },

    // For internal functions
    { "aaLen", "_aaLen" },
    { "aaKeys", "_aaKeys" },
    { "aaValues", "_aaValues" },
    { "aaRehash", "_aaRehash" },
    { "monitorenter", "_d_monitorenter" },
    { "monitorexit", "_d_monitorexit" },
    { "criticalenter", "_d_criticalenter" },
    { "criticalexit", "_d_criticalexit" },

    // For pragma's
    { "GNU_asm" },
    { "lib" },
    { "msg" },
    { "startaddress" },

    // For special functions
    { "tohash", "toHash" },
    { "tostring", "toString" },
    { "getmembers", "getMembers" },

    // Special functions
    { "alloca" },
    { "main" },
    { "WinMain" },
    { "DllMain" },
    { "tls_get_addr", "___tls_get_addr" },

    // Builtin functions
    { "std" },
    { "math" },
    { "sin" },
    { "cos" },
    { "tan" },
    { "_sqrt", "sqrt" },
    { "fabs" },

    // Traits
    { "isAbstractClass" },
    { "isArithmetic" },
    { "isAssociativeArray" },
    { "isFinalClass" },
    { "isFloating" },
    { "isIntegral" },
    { "isScalar" },
    { "isStaticArray" },
    { "isUnsigned" },
    { "isVirtualFunction" },
    { "isAbstractFunction" },
    { "isFinalFunction" },
    { "hasMember" },
    { "getMember" },
    { "getVirtualFunctions" },
    { "classInstanceSize" },
    { "allMembers" },
    { "derivedMembers" },
    { "isSame" },
    { "compiles" },
};


int main()
{
    FILE *fp;
    unsigned i;

    {
	fp = fopen("id.h","w");
	if (!fp)
	{   printf("can't open id.h\n");
	    exit(EXIT_FAILURE);
	}

	fprintf(fp, "// File generated by idgen.c\n");
#if __DMC__
	fprintf(fp, "#pragma once\n");
#endif
	fprintf(fp, "#ifndef DMD_ID_H\n");
	fprintf(fp, "#define DMD_ID_H 1\n");
	fprintf(fp, "struct Identifier;\n");
	fprintf(fp, "struct Id\n");
	fprintf(fp, "{\n");

	for (i = 0; i < sizeof(msgtable) / sizeof(msgtable[0]); i++)
	{   const char *id = msgtable[i].ident;

	    fprintf(fp,"    static Identifier *%s;\n", id);
	}

	fprintf(fp, "    static void initialize();\n");
	fprintf(fp, "};\n");
	fprintf(fp, "#endif\n");

	fclose(fp);
    }

    {
	fp = fopen("id.c","w");
	if (!fp)
	{   printf("can't open id.c\n");
	    exit(EXIT_FAILURE);
	}

	fprintf(fp, "// File generated by idgen.c\n");
	fprintf(fp, "#include \"id.h\"\n");
	fprintf(fp, "#include \"identifier.h\"\n");
	fprintf(fp, "#include \"lexer.h\"\n");

	for (i = 0; i < sizeof(msgtable) / sizeof(msgtable[0]); i++)
	{   const char *id = msgtable[i].ident;
	    const char *p = msgtable[i].name;

	    if (!p)
		p = id;
	    fprintf(fp,"Identifier *Id::%s;\n", id);
	}

	fprintf(fp, "void Id::initialize()\n");
	fprintf(fp, "{\n");

	for (i = 0; i < sizeof(msgtable) / sizeof(msgtable[0]); i++)
	{   const char *id = msgtable[i].ident;
	    const char *p = msgtable[i].name;

	    if (!p)
		p = id;
	    fprintf(fp,"    %s = Lexer::idPool(\"%s\");\n", id, p);
	}

	fprintf(fp, "}\n");

	fclose(fp);
    }

    return EXIT_SUCCESS;
}
