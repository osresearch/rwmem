Read and Write physical memory on OS X
===

*This ~~can~~ WILL crash your machine!*

The purpose of this tool is to read and write physical memory addresses
of the running system.  It is possible to crash the machine by writing
to arbitrary pages, corrupt the kernel, mess up memory mappings, etc.

Loading the `DirectHW.kext` gives any root process the ability to
poke anywhere on the system.  It is basically a deliberate backdoor
in the kernel.  You can download it from Snare's site, if you trust him:
http://ho.ax/downloads/DirectHW.dmg

