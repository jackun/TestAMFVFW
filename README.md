# TestAMFVFW

Testing, testing...

VFW glue using AMD's Media SDK (AMF) aka encode video using VCE.

**You may need to install [MSVC++ 2013 runtimes](http://www.microsoft.com/en-us/download/details.aspx?id=40784).**

**NOTE: You need to install x86 version of the runtime for 32bit codec even if your Windows is 64 bit.**


# WIP!!!
Can get only around 45 fps with 1080p on R9 290 for some reason.

Atleast Dxtory 2.0.130 for some reason doesn't seem to specify encode fps (through ICM_COMPRESS_FRAMES_INFO message). I've defaulted to 30 fps but you can override it.
