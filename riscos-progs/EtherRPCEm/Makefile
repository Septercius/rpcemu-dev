# Project:   EtherRPCEm


# Toolflags:
CCflags = -depend !Depend -IC: -throwback -zM -Wp
Linkflags = -rmf
ObjAsmflags = -throwback -NoCache -depend !Depend
CMHGflags = -d h.ModHdr  -throwback
LibFileflags = -c -o $@
Squeezeflags = -o $@

OBJS = o.Module o.errors o.intveneer o.ModHdr

# Final targets:
EtherRPCEm: $(OBJS)
	Link $(Linkflags) -o $@ $(OBJS) C:o.stubsg

.c.o:
	cc $(CCflags) -c -o $@ $<

.s.o:
	objasm $(ObjAsmflags) -o $@ $<

# User-editable dependencies:
clean:
	remove c.AutoSense
	remove h.ModHdr
	remove o.errors
	remove o.intveneer
	remove o.ModHdr
	remove o.Module

c.AutoSense: AutoSense.EtherRPCEm
	bin2c -t "const unsigned char" -n autosense_file $? $@

o.Module: c.AutoSense

# A dependency with dummy action, because the cmhg file produces two
# outputs (h.ModHdr and o.ModHdr)
h.ModHdr: cmhg.ModHdr
	|echo

o.ModHdr: cmhg.ModHdr
	cmhg $(CMHGflags) cmhg.ModHdr -o o.ModHdr


# Dynamic dependencies:
o.intveneer: s.intveneer
o.errors: s.errors
o.Module:	c.Module
o.Module:	C:h.kernel
o.Module:	C:h.swis
o.Module:	h.ModHdr
o.Module:	h.dcistructs
o.Module:	h.Defines
o.Module:	h.errors
o.Module:	C:h.kernel
o.Module:	h.Structs
o.Module:	h.dcistructs
o.Module:	h.mbuf_c
o.Module:	c.AutoSense
