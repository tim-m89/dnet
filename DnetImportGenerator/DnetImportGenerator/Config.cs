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
using System.Reflection;
using System.IO;


namespace DnetImportGenerator
{
    /* Find all assemblies and create a file to specifiy which assemblies should be used by the import gen */
    class Config
    {
        static StreamWriter configFile;

        public static void createConfig()
        {
            configFile = new StreamWriter(new FileStream("./assemblylist", FileMode.Create, FileAccess.Write));
            string[] gac = getGAC();
            foreach (string s in gac)
            {
                DirectoryInfo di = new DirectoryInfo(s);
                if (di.Exists)
                    searchDirectory(di);
            }
            configFile.Close();
        }

        static void searchDirectory(DirectoryInfo di)
        {
            FileSystemInfo[] fsis = di.GetFileSystemInfos();
            foreach (FileSystemInfo fsi in fsis)
            {
                if ((fsi is FileInfo) && (fsi.Extension == ".dll"))
                    checkAssemblyFile((FileInfo)fsi);
                else if (fsi is DirectoryInfo)
                    searchDirectory((DirectoryInfo)fsi);
            }
        }
        
        static void checkAssemblyFile(FileInfo fi)
        {
            try
            {
                Assembly assem = Assembly.ReflectionOnlyLoadFrom(fi.FullName);
                string vers = "v2.0.50727"; //runtime version used by 2.0, 3.0, 3.5. newer like the 4.0 beta throw a BadImageFormatException but it is nice to check aswell
                if (assem.ImageRuntimeVersion == vers)
                    writeToConfig(assem);
            }
            catch (BadImageFormatException) { }
        }

        static string[] getGAC()
        {
            if (Type.GetType("Mono.Runtime") == null)
            {
                string windir = Environment.GetEnvironmentVariable("windir");
                return new string[] { windir + "\\assembly\\GAC_32\\", windir + "\\assembly\\GAC_MSIL\\" };
            }
            else
            {
                string monohome = Environment.GetEnvironmentVariable("MONO_HOME");
                return new string[] { monohome + "/lib/mono/gac" };
            }
        }

        static void writeToConfig(Assembly assem)
        {
            if (!assem.FullName.StartsWith("mscorlib"))
                configFile.Write("//");
            configFile.WriteLine(assem.FullName);
            Console.WriteLine(assem.FullName.Remove(assem.FullName.IndexOf(',')));
        }

    }
}
