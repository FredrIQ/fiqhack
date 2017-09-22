About this document:
Last modified by Fredrik Ljungdahl, 2015-11-03

Copyright (C) 2017 Fredrik Ljungdahl.

This README file is licensed under the NetHack General Public License.  See
libnethack/dat/license for details.


Overview
========

NetHack is free software, and has no warranty of any sort. For more
information on the license situation, read `COPYING` (a summary), or
`copyright` (full details).


NetHack 4
=========

NetHack 4 is a version of the computer game NetHack, that aims to bring a
better-quality codebase and a less hostile interface to NetHack's highly rated
"roguelike" gameplay.  Unlike many NetHack variants, it is intentionally
conservative in making gameplay changes; in particular, it aims to avoid the
common problem whereby development is driven via fixing perceived balance
issues with the game (often losing the ability to improvise solutions to
complex problems in the process), or via focusing too much on the difference
between wins and losses (meaning that players are forced into taking the best
choice, rather than the choice they enjoy most or a choice they want to try).
See the file `doc/philosophy.txt` for more information on NetHack 4's
philosophy.


FIQHack
=======

FIQHack is a fork of NetHack4, aimed at improving balance at places, test out
new things, improving the codebase with internal changes and to promote
consistency between how players and monsters work, and to improve the monster AI.

More detailed information can be seen in `doc/CHANGELOG`.


This repository
===============

This repository consists of the following branches

 * A master branch exist containing general experimental in-development
   changes which is unfinished and yet to be stable and ready for the
   public. Saves can break at any moment.

 * One-off feature branches, for developing a certain new feature or
   refactor

 * Branches representing a certain version, used to push changes
   specific to a certain version. With the exception of critical bug
   fixes, changes are only pushed to the latest of the versions.


Build Instructions
==================

Dependencies
------------

To build FIQHack, you need make, zlib (which is probably already installed),
libjansson (http://www.digip.org/jansson -- shipped with this source), and
its development branch. You will also need `bison` and `flex`, as well as
a C compiler (gcc or clang will both work).

The default setup is to install to `~/fiqhackdir`. If you want to change
this, you can modify `GNUmakefile` -- file paths are set at the top.
Once you are happy with the setup, to compile, you just do

`make && make install`

In case this doesn't work, please file an issue describing your error,
perhaps documentation is lacking or something is missing. Enjoy!


Old NetHack 4 instructions with aimake
--------------------------------------

The build instructions in this system are basic "do this, and things should
work" advice, and do not go heavily into details of customizing a build.

If you want to do something complex or unusual, you can read the build
system manual (which lists everything in more detail than most people will
need) via running the command

    perl aimake --documentation

UNIX/Linux/Mac OS X
-------------------

You will need to install FIQHack's dependencies: zlib (which is probably
installed already, but you may need to get its development headers from your
package manager), and (if you want a working server binary) inetd, postgresql
and pgcrypto.  You also need development headers for the libraries listed.

FIQHack also requires a working libjansson library (available from
http://www.digip.org/jansson), and its development headers.  However, both of
those ship with NetHack 4, and will be used by default or if you explicitly
specify `--with=jansson` as an option to `aimake`.  (If you have installed
libjansson yourself, you can give `--without=jansson` to use your own copy.
This will build a little faster and avoid installing redundant copies of the
jansson library.)

You will also need the `bison` and `flex` programs (in addition to the usual
compilers), because some of NetHack's tools (e.g. the level-handling
utilities) are written in them.

If you want to build the tiles or faketerm ports (the default), you will also
need to install libpng, and version 2 of the Simple DirectMedia Layer; it may
be available in your package manager, or else you could download the source
code from http://www.libsdl.org and compile it yourself.  Otherwise, you can
specify `--without=gui` as an option to `aimake` to build just the console
port (which is built either way), and disable the tiles ports.  Note that the
tiles ports do not currently work on Mac OS X.

Assuming you just want to run FIQHack from your home directory, from the top
source directory, run:

    mkdir build
    cd build
    ../aimake -i ~/fiqhack .. # or wherever you want

If you want to install for all users, you will need to tell `aimake` which
location to install it into, and how to elevate its permissions:

    # as a regular user, not root
    mkdir build
    cd build
    ../aimake -i /usr/local -S su ..    # or perhaps -S sudo

Note that this requires a group `games` to exist on your system, and contain
no normal users, in order to ensure security of the bones files and high score
tables.  This is the case on most Linux distributions, but not on all, and may
not be the case on UNIX.

To run the console port, use `fiqhack`.  For the tiles/faketerm ports, use
`fiqhack-sdl`.


Windows
-------

You will need to install various prerequisite programs in order to compile
NetHack 4.  The build system is written in Perl, and as such, the simplest way
to get a working toolchain is to install Strawberry Perl, available at
http://strawberryperl.com, which comes with a working C toolchain.  You will
also need to install Flex and Bison, scanner and parser generators; the
versions at GnuWin32 (http://gnuwin32.sourceforge.net/) work (although they
don't have a very Windows-like idea of directory structure, and thus can't be
installed to paths with spaces in, and produce compiler warnings; if you know
a better option, let us know).  Strawberry Perl's and GnuWin32's executables
will all need to be on your PATH (search for "PATH" in Control Panel, on
recent versions of Windows).

You will also need to create two more folders, `build` and `install`; I
recommend that both are parallel to the `nethack4` folder that contains the
entire NetHack 4 distribution.

If you want to build a tiles or fake terminal port (recommended even if you
aren't a tiles player, because Windows' terminal is rather slow), you will
need version 2 of the Simple DirectMedia Layer.  Download the MinGW version of
the development headers and import libraries from http://www.libsdl.org; also
download the library itself.  Then copy the entire `include/SDL2` subdirectory
from the appropriate processor-dependent directory of the SDL distribution, to
the `c/include` folder that was created when you installed Strawberry Perl (so
that it beomes `c/include/SDL2`); and all the files `lib/*.a` from the
appropriate processor-dependent directory of the SDL distribution to the
`c/lib` folder that was created when you installed Strawberry Perl (so that
they become `c/lib/libSDL2.a`, etc.).  Finally, copy the file `SDL2.dll` that
you obtained when you downloaded the library itself to the `prebuilt` folder
inside the `nethack4` folder that contained the distribution.

Once you've done this, open Strawberry Perl's command prompt, change to the
build directory, and type:

    perl ..\nethack4\aimake -i ..\install --directory-layout=single_directory

aimake should compile and install the entirety of NetHack 4 for you into the
install directory.  In order to run the console port, change directory to the
install directory, and type `nethack4`.  For the tiles/faketerm port, use
`nethack4-sdl.exe`.

It is possible that `ld.exe` will crash in the process of the build.  This
only seems to happen after it has already produced the output that aimake
needed, so you can just dismiss the error box and let the build work as
normal.

Note that despite the best efforts of the rendering library, the game is quite
slow to render on the console when using recent versions of Windows; this is
because the Windows console itself is prety slow.  (For a comparison, you can
try running a command that produces a lot of text, such as `perl
../nethack4/aimake --documentation`, and observing how long it takes to scroll
the screen when you press the spacebar.)  The faketerm port somehow manages to
be faster, even though it too is fairly slow.


Server setup
------------

If you want to run your own server (which is only necessary/useful if you want
people to be able to connect to your NetHack 4 server from other computers,
rather than running locally), you'll need to give `--with=server` as a
command-line option to `aimake`, and also need to set up the postgresql
database:

    su postgres             # or any other way to elevate your permissions
    createuser -DPRS nh4server
    # You'll be prompted for a password at this point.
    createdb -O nh4server nethack4
    echo 'CREATE EXTENSION pgcrypto' | psql -d nethack4
    exit                    # go back to your normal permissions

Next you need to edit the configuration file (a blank configuration file will
have been installed in the appropriate place for you to edit).  If you
installed into `~/nethack4`, it should be named
`~/nethack4/config/nethack4.conf`; other forms of install may have other
locations.  (You can run nethack4-server with no arguments to discover where
the configuration file should be; if it can't find its configuration file,
it'll complain and tell you where it's looking for it.)

The configuration file looks something like this:

    dbhost=127.0.0.1
    dbport=5432
    dbuser=nh4server
    dbpass=**password**
    dbname=nethack4

Note that the port number has been known to vary based on the way that your
copy of postgresql is packaged; you may want to verify it by looking at
postgresql's configuration, `/etc/postgresql/.../postgresql.conf`.  Also be
aware that the configuration file necessarily has to store the password in
plaintext (a hashed password is no good for actually logging into the
database); you may want to change the permissions on the configuration file to
help protect it.  (I recommend using a long random password, because it's only
used by computers; there's no need for humans to memorize it.)

Finally, you need to tell inetd about the new server setup.  As root, you need
to add two extra lines to `/etc/inetd.conf`, looking something like this:

    53430 stream tcp  nowait username /path/to/nethack4-server nethack4-server
    53430 stream tcp6 nowait username /path/to/nethack4-server nethack4-server

(Here, "username" is the username of the user that the server binaries should
run as.)  Then, again as root, just send a `SIGHUP` to inetd (`killall -HUP
inetd`), to tell it to load the new configuration file.  inetd will be
responsible for starting the server processes when a new connection is made;
no server processes will be running while no games are being played.  To test
your server setup, you can use the `nethack4` client; there's a menu option to
connect to a server with it.

This only really works properly on Linux, at present; on Mac OS X, it may be
possible to get a partially working server, but functionality is missing due
to that operating system's lack of support for realtime signals.
