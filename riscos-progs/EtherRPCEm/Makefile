# Project:   EtherRPCEm


# Toolflags:
CCflags = -c -depend !Depend -IC: -throwback -zM -Wp
C++flags = -c -depend !Depend -IC: -throwback -zM 
Linkflags = -rmf -c++ -o $@ 
ObjAsmflags = -throwback -NoCache -depend !Depend 
CMHGflags = -d h.ModHdr  -throwback 
LibFileflags = -c -o $@
Squeezeflags = -o $@


# Final targets:
@.!System.310.Modules.Network.EtherRPCEm:   @.o.Module C:o.stubs @.o.intveneer @.o.ModHdr
        Link $(Linkflags) @.o.Module C:o.stubs @.o.intveneer @.o.ModHdr


# User-editable dependencies:
clean:
	remove h.ModHdr
	remove o.intveneer
	remove o.ModHdr
	remove o.Module

# Static dependencies:
@.o.Module:   @.c.Module
        cc $(ccflags) -o @.o.Module @.c.Module 
@.o.intveneer:   @.s.intveneer
        objasm $(objasmflags) -from @.s.intveneer -to @.o.intveneer

# A dependency with dummy action, because the cmhg file produces two
# outputs (h.ModHdr and o.ModHdr)
h.ModHdr: cmhg.ModHdr
	|echo

@.o.ModHdr:   @.cmhg.ModHdr
        cmhg $(cmhgflags)  @.cmhg.ModHdr -o @.o.ModHdr 


# Dynamic dependencies:
o.intveneer: s.intveneer
o.intveneer: h.Equates
o.Module:	c.Module
o.Module:	C:h.kernel
o.Module:	C:h.swis
o.Module:	h.ModHdr
o.Module:	h.Defines
o.Module:	h.Structs
o.Module:	h.mbuf_c
o.Module:	h.DCI
o.Module:	h.Module
o.Module:	h.DCI
