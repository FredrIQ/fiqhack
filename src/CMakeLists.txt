# src/CMakeLists.txt : build libnethack

set (LIBNETHACK_SRC
    allmain.c  apply.c    artifact.c attrib.c   ball.c    bones.c
    botl.c     cmd.c      dbridge.c  decl.c     detect.c  dig.c      display.c
    dlb.c      do.c       dog.c      dogmove.c  dokick.c  do_name.c  dothrow.c
    do_wear.c  drawing.c  dungeon.c  eat.c      end.c     engrave.c  exper.c
    explode.c  extralev.c files.c    fountain.c hack.c    hacklib.c  invent.c
    light.c    lock.c     locking.c  makemon.c  mcastu.c  mhitm.c    mhitu.c
    minion.c   mklev.c    mkmap.c    mkmaze.c   mkobj.c   mkroom.c   mon.c
    mondata.c  monmove.c  monst.c    mplayer.c  mthrowu.c muse.c     music.c
    objects.c  objnam.c   o_init.c   options.c  pager.c   pickup.c   pline.c
    polyself.c potion.c   pray.c     priest.c   quest.c   questpgr.c read.c
    rect.c     region.c   restore.c  rnd.c      role.c    rumors.c   save.c
    shk.c      shknam.c   sit.c      sounds.c   spell.c   sp_lev.c   symclass.c
    steal.c    steed.c    teleport.c timeout.c  topten.c  track.c    trap.c
    uhitm.c    u_init.c   vault.c    version.c  vision.c  weapon.c   were.c
    wield.c    windows.c  wizard.c   worm.c     worn.c    write.c    xmalloc.c zap.c
    )
set (LIBNETHACK_GENERATED_SRC
    ${NetHack_BINARY_DIR}/src/monstr.c
    )

set_source_files_properties(${LIBNETHACK_GENERATED_SRC} GENERATED)


add_custom_command (OUTPUT ${NetHack_BINARY_DIR}/src/monstr.c
                    COMMAND ${NetHack_BINARY_DIR}/util/makedefs
                    ARGS -m ${NetHack_BINARY_DIR}/src/monstr.c
                    DEPENDS makedefs)

include_directories (
    ${NetHack_SOURCE_DIR}/include
    ${NetHack_SOURCE_DIR}/include/public
    ${NetHack_BINARY_DIR}/include )

#add_definitions ()
add_definitions (-Wall -fvisibility=hidden -g3 -Wno-unused-but-set-variable -Wold-style-definition -Wstrict-prototypes)
add_library(nethack SHARED ${LIBNETHACK_SRC} ${LIBNETHACK_GENERATED_SRC})

add_dependencies (nethack makedefs_headers)
