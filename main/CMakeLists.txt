set(Z80PACK ${CMAKE_SOURCE_DIR}/../z80pack)

idf_component_register(
	SRCS
		"cydsim.c"
		"disks.c"
		"simcfg.c"
		"simio.c"
		"simmem.c"
		"${Z80PACK}/iodevices/rtc80.c"
		"${Z80PACK}/iodevices/sd-fdc.c"
		"${Z80PACK}/z80core/sim8080.c"
		"${Z80PACK}/z80core/simcore.c"
		"${Z80PACK}/z80core/simdis.c"
		"${Z80PACK}/z80core/simglb.c"
		"${Z80PACK}/z80core/simice.c"
		"${Z80PACK}/z80core/simz80-cb.c"
		"${Z80PACK}/z80core/simz80-dd.c"
		"${Z80PACK}/z80core/simz80-ddcb.c"
		"${Z80PACK}/z80core/simz80-ed.c"
		"${Z80PACK}/z80core/simz80-fd.c"
		"${Z80PACK}/z80core/simz80-fdcb.c"
		"${Z80PACK}/z80core/simz80.c"
	INCLUDE_DIRS
		"."
		"${Z80PACK}/iodevices"
		"${Z80PACK}/z80core"
)
