ZIP UTILS
-----------
by Lucian Wischik, June 2004 - July 2005


These files are a simple way to add zip/unzip functionality to
your windows programs, both win32 and windows-ce.


For unzipping, add "unzip.cpp" to your project. Then, for example,
  #include "unzip.h"
  //
  HZIP hz = OpenZip("c:\\stuff.zip",0);
  ZIPENTRY ze; GetZipItem(hz,-1,&ze); int numitems=ze.index;
  for (int i=0; i<numitems; i++)
  { GetZipItem(hz,i,&ze);
    UnzipItem(hz,i,ze.name);
  }
  CloseZip(hz);




For zipping, add "zip.cpp" to your project. (You can add just one of
zip/unzip, or both; they function independently and also co-exist.)
  #include "zip.h"
  //
  HZIP hz = CreateZip("c:\\simple1.zip",0);
  ZipAdd(hz,"znsimple.bmp", "c:\\simple.bmp");
  ZipAdd(hz,"znsimple.txt", "c:\\simple.txt");
  CloseZip(hz);



There is lot of flexibility... When you unzip, the zipfile can
be a handle or a file on disk or a block of memory or a resource.
And when you unzip the items, you can unzip them directly
to a handle or diskfile or pipe or block of memory. You might
even spawn off a thread to play wav files, connected via a pipe,
and unzip directly from a resource into that pipe.
Similarly you can create a zip in different ways (even creating it
as a dynamically growable buffer that's backed by the system paging
file), and the items you add can come from anywhere.

The functions all support unicode filenames. And the final '0' argument
in OpenZip/CreateZip is for a password, in case of encryption.

For more information, see the header files or the example source code.
The example comes with project files for Visual Studio .NET and
Embedded Visual C++3 and Borland C++Builder6. 


The actual core source code for zipping and unzipping comes from
www.info-zip.org and www.gzip.org/zlib, by Jean-Loup Gailly and Mark
Adler and others, and is freely available at the respective
websites. All I did was repackage them and add some small extras.

