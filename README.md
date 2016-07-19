# TestAMFVFW

Testing, testing...

VFW glue using AMD's Media SDK (AMF) aka encode video using VCE.

# Installing

Click `Download ZIP` and unpack somewhere permanent. Codec needs to find the AMF DLLs from the `Runtimes` folder.

**You may need to install [MSVC++ 2015 runtimes](https://www.microsoft.com/en-us/download/details.aspx?id=48145).**

**NOTE: You need to install x86 version of the runtime for 32bit codec even if your Windows is 64 bit.**


# WIP!!!

Check "Disable OpenCL" to use DX11 Direct Compute for NV12 conversion, seems faster. Doesn't work on Windows 7 though.

Can get only around 45 fps with 1080p on R9 290 for some reason.

Atleast Dxtory 2.0.130 for some reason doesn't seem to specify encode fps (through ICM_COMPRESS_FRAMES_INFO message). I've defaulted to 30 fps but you can override it.
