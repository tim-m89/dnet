/********************************************************************
 *       This file is distributed under the Microsoft Public        *
 *       License (Ms-PL). See license.txt for details.              *
 *       Copyright (c) 2009 Tim Matthews & Cristian L. Vlasceanu    *
 *       All rights reserved                                        *
 * *****************************************************************/

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace DnetImportGenerator
{
    enum ImportType
    {
        None = 0, //Should be semantically the same as private
        Private = 1,
        Public = 2
    }

    class DImport
    {
        public string name = null;

        public DImport(string ns)
        {
            this.name = ns;
        }
        public override string ToString()
        {
            ImportType impType = Generator.getImportType();
            string s = null;
            if(impType== ImportType.Public)
                s += "public ";
            else if(impType==ImportType.Private)
                s += "private ";
            s += "static import " + name + ";";
            return s;
        }

    }
}
