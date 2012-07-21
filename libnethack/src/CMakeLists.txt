# src/CMakeLists.txt : build libnethack

set (LIBNETHACK_SRC
    allmain.c  apply.c    artifact.c attrib.c   ball.c    bones.c
    botl.c     cmd.c      dbridge.c  decl.c     detect.c  dig.c      display.c
    dlb.c      do.c       dog.c      dogmove.c  dokick.c  do_name.c  dothrow.c
    do_wear.c  drawing.c  dump.c     dungeon.c  eat.c     end.c      engrave.c  exper.c
    explode.c  extralev.c files.c    fountain.c hack.c    hacklib.c  history.c invent.c
    light.c    lock.c     log.c      logreplay.c makemon.c mcastu.c  memfile.c mhitm.c    mhitu.c
    minion.c   mklev.c    mkmap.c    mkmaze.c   mkobj.c   mkroom.c   mon.c
    mondata.c  monmove.c  monst.c    mplayer.c  mthrowu.c mtrand.c   muse.c     music.c
    objects.c  objnam.c   o_init.c   options.c  pager.c   pickup.c   pline.c
    polyself.c potion.c   pray.c     priest.c   quest.c   questpgr.c read.c
    rect.c     region.c   restore.c  role.c     rumors.c  save.c
    shk.c      shknam.c   sit.c      sounds.c   spell.c   sp_lev.c   symclass.c
    steal.c    steed.c    teleport.c timeout.c  topten.c  track.c    trap.c
    uhitm.c    u_init.c   vault.c    version.c  vision.c  weapon.c   were.c
    wield.c    windows.c  wizard.c   worm.c     worn.c    write.c    xmalloc.c zap.c
    )
set (LIBNETHACK_GENERATED_SRC
    ${LNH_SRC_GEN}/monstr.c
    )

set_source_files_properties(${LIBNETHACK_GENERATED_SRC} GENERATED)

get_property(MAKEDEFS_BIN TARGET makedefs PROPERTY LOCATION)


add_custom_command (OUTPUT ${LNH_SRC_GEN}/monstr.c
                    COMMAND ${MAKEDEFS_BIN}
                    ARGS -m ${LNH_SRC_GEN}/monstr.c
                    DEPENDS makedefs)

include_directories (
    ${NetHack4_SOURCE_DIR}/libnethack/include
    ${NetHack4_SOURCE_DIR}/include
    ${NetHack4_BINARY_DIR}/libnethack/include )

add_library(libnethack ${LIB_TYPE} ${LIBNETHACK_SRC} ${LIBNETHACK_GENERATED_SRC})
set_target_properties(libnethack PROPERTIES OUTPUT_NAME nethack )
target_link_libraries(libnethack z)

add_dependencies (libnethack makedefs_headers)

install(TARGETS libnethack
        DESTINATION ${LIBDIR})
