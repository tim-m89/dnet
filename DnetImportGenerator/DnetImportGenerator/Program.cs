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
using System.IO;

namespace DnetImportGenerator
{
    class Program
    {
        static void Main(string[] args)
        {
            if (args.Length == 0)
            {
                Console.WriteLine("usage:");
                Console.WriteLine("-l generate a list of assemblies as ./assemblylist. This file should then be edited. Remove the leading '//' for each assembly you want to use");
                Console.WriteLine("-i generate the actual import files from the assembly list");
                return;
            }
            
            bool assemList = false;
            bool importGen = false;
            foreach (string arg in args)
            {
                if (arg == "-l")
                    assemList = true;
                if (arg == "-i")
                    importGen = true;
            }


            if (assemList)
            {
                Console.WriteLine("Finding assemblies");
                Config.createConfig();
            }
            if (importGen)
            {
                Console.WriteLine("Generating import files");
                Generator.GenerateDImports(@"./dnet-imports", true, true);
            }
        }
    }
}
