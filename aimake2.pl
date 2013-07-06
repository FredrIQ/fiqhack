#!/usr/bin/perl
use utf8;

=encoding utf8

=head1 NAME

aimake - build and/or install C programs with minimal configuration

=head1 SYNOPSYS

B<mkdir> F<build>; B<cd> F<build>; B<perl> F<../aimake> B<-i> F<installdir>

=head1 DESCRIPTION

B<aimake> is a build system for C programs that attempts to deduce as much as
possible itself, rather than requiring a separate file along the lines of
C<Makefile>, C<config.ac>, or C<CMakeLists.txt>. In the most common cases, no
configuration file is required at all, although a rudimentary configuration
file can be used to specify more complicated things that cannot be
automatically deduced (such as which files contain the entry points to
libraries, or what commands to use to generate generated source files).

To compile a project with aimake for the first time, create an empty
directory, then call aimake from that directory. By default, it will compile
the source files in the same directory tree as F<aimake> itself. You can use a
version of aimake other than the version shipped with the project by running
your version of aimake (from the build directory), and giving the path to the
root of the project's distribution as an argument. To rebuild the project,
call aimake with no arguments in your build directory. To install it, call
aimake with the B<-i> switch, and specify where you want to install it (you
can use just B<-i> by itself to install in the same place as last time).

=head1 OPTIONS

=over 4

=item B<-v> I<number>

Set how verbose the build should be. The default level is 1; higher values
produce more verbose output. The messages you get at each verbosity level are:

=over 4

=item -1

No messages at all. Even important ones.

=item 0

Messages about warnings, errors, and failures to compile. No messages will be
printed if everything works correctly.

=item 1

Level 0 messages, plus messages about operations that aimake is performing
that actually contribute to compiling the code (e.g. running compilers,
linkers, and so on). At level 1 and above, aimake also displays the details of
what it is thinking temporarily, using one line that repeatedly changes.

=item 2

Level 1 messages, plus messages about other operations that aimake performs
(such as determining dependencies).

=item 3

Level 2 messages, plus a record of all the actual commands that are run.
Additionally, the temporary messages about what aimake is doing are left on
the screen, rather than removed.

=item 4

Level 3 messages, plus information useful mostly for debugging aimake itself.

=back

=item B<-i> F<directory>

After building (if necessary), install the project into the given
directory. The directory can be omitted, in which case it will use the same
install directory as last time.

=item B<-p> F<directory>

When installing, treat the given directory as the root directory, rather than
using the actual root directory. (For instance, giving B<-i> F</usr> B<-p>
F</home/user> would install into F</home/user/usr>, but the installed program
would look for paths as if it had been installed into F</usr>.) This is
intended for use in packaging programs, and when installing into a chroot from
outside the chroot.

=item B<-d>

Instead of compiling, dump the internal statefile to stdout in a
human-readable format. This is mostly only useful for debugging aimake.

=item B<-W> I<regexp>

Instead of compiling, output all error and warning messages that were obtained
during the last compile, for files matching the given regexp. (Don't include
the // around the regexp.)

=item B<-r>

Don't use carriage returns to provide progress information (mostly only useful
if stdout is not a terminal).

=item B<-B> I<regexp>

Consider all files matching the regexp to have changed, even if they actually
haven't. (Don't include the // around the regexp.)

=back

=head1 INTERNALS

=head2 THEORY

As input, we have a bunch of files (the objects that aimake starts with), and
a list of instructions for what objects can be made from what other objects,
together with any state information from a previous run. If no target is
specified, we want to deduce what the default targets are, then build them.

For each object, we have commands to determine what it depends on, and what it
provides and/or produces ("provices"). If an object A provides another object
B, that means that if anything depends on B, then A can be used instead to
fulfil the dependency. (For instance, the system's libc can fulfil a
dependency on C<symbol:exit>.) Things that are provided are abstract concepts
like files with a particular name, or functions or variables, that don't
necessarily correspond to a single particular file on the filesystem. If an
object A produces another object B, that means that given A (and possibly
other objects: A's use dependencies (and their use dependencies, and so on)),
it's possible to create/recreate B. Things that are produced are physical
files on the filesystem (typically in the build directory).

Most make programs build a graph of objects and their dependencies, and then
update objects if any of their dependencies are out of date (recursively
updating their dependencies first). aimake works on a similar algorithm, but
complicating the problem is the fact that dependencies might not be known in
advance. As such, produced objects become marked as needing a rebuild if any
of their dependencies, or anything providing any of their dependencies, have
changed since they were last built, or if they have different dependencies
since they were last built. (Provided objects don't exist on the filesystem,
and as such cannot be rebuilt.) Whenever an object is rebuilt, its provisions
and productions (collectively, "provicions") are recalculated.

A "correct" but very inefficient algorithm is "repeatedly, look for a rule
where some of the dependencies have changed and run it". The reason this is
inefficient is that if multiple dependencies of an output object are out of
date, it will potentially be rebuilt once for every file it depended on that
changed. Only file-like objects can be outputted this way; everything else
exists only to calculate dependencies.

As such, to optimize the process, a cache is maintained, for each rule/inputs
combination, of the file-like dependencies that were used the last time the
rule was run with those inputs. If any dependencies of the object's
dependencies have changed, or any dependencies of those, and so on, going back
indefinitely, we don't want to update that object immediately (unless the
change caused no change to the dependency itself). In addition to this, we try
to group uses of the same rule (for neatness and so that if several
alternatives are being generated for a symbol, they tend to be generated at
around the same time), and compile objects in alphabetical order (for
neatness).

Finally, at the end of each run, any generated file-like objects that couldn't
be built are simply deleted (from the filesystem in the case of things in the
build tree, or just from aimake's internal state otherwise).

Any errors and warnings are fed back to the user. Warnings are simple enough,
because they correspond to commands that succeeded and produced something, and
so the warnings can be cached along with the produced object. Errors are also
simple when an attempt to produce an object fails; they can also be cached,
because if a rule fails, it's going to continue failing unless something
relevant is changed (like one of the input sor the rule itself). However,
another common sort of error is if an object can't be produced at all. Due to
the forward-chaining aimake uses, it is not immediately clear whether a rule
not matching is a result of something the user cares about or not. The rule
aimake uses is to report the failure to apply a rule if all the rule's listed
dependencies were fulfilled, but some dependencies implied in via use
dependencies weren't fulfilled.

=head2 AIMAKE OBJECTS

aimake tracks a lot more types of objects than just files:

=over 4

=item C<path:>

One of the files in the source directory. (aimake is designed for out-of-tree
builds, although it can build into a subdirectory of the source directory; it
simply treats any subdirectory that contains an aimake state file as not
existing.) aimake scans these for changes. This is a relative, not absolute,
path (so that it continues to work correctly if the source directory is
renamed), in aimake relative path format (that is, directories are separated
with slashes, and colons, slashes and backslashes in path components are
escaped with backslashes; note that aimake relative path format cannot
represent an absolute path, which saves having to worry about how the
filesystem is rooted). Because they represent specific files, C<path:> objects
are produced, not provided.

=item C<bpath:>

One of the files in the build directory: things like object files. Like with
C<path:>, this is a relative path in aimake relative path format. C<bpath:>
objects are produced, not provided.

=item C<spath:>

One of the files that exists as part of the system's toolchain: compilers,
system libraries, system headers, and the like. This is always an absolute
path, and is in aimake's absolute path format (which works like its relative
path format, except that it has an extra component at the start, which
represents the filesystem root to use in a system-dependent manner; this might
be the null string on systems whose filesystem only has one root).

C<spath:> objects can only be created via things external to aimake (such as
the system's package manager), but they can be produced via a no-op command,
to tell aimake to consider them to be relevant.

=item C<searchpath:>

A directory that is looked in for include files, libraries, or the like. This
is formed out of a namespace and a file-like object (typically an C<spath:>,
because include files and libraries that are inside the source directory would
be found anyway), and looks something like
C<searchpath:library:spath:/usr/lib>. Search paths are generally used to
"produce" the includes or libraries that they contain. (Of course, they're
already existing, but that doesn't matter to aimake; it just cares that after
running the production command, the file it's meant to produce exists.)

=item C<extend:>

A file that's produced from another file. This is formed out of an extension
and a file-like object, and is just an alias for something in the C<bpath:>.
For instance, C<extend:.s:path:src/foo.c> might be an alias for
C<bpath:src/foo.s>. As a special case, an extension of ".." causes the
filename to be deleted too, producing the directory of the object:
C<extend:..:path:src/foo.c> is C<path:src>. This construction might produce an
C<spath:>, C<path:> or C<bpath:>.

=item C<file_required_by:>

A reference to a file with a particular name, and the object that requires
it. This sort of object only makes sense in dependencies, and looks something
like C<file_required_by:stdio.h:path:foo.c> (that is, "the F<stdio.h> that's
required to build something that uses F<foo.c>"). The name of the object with
the dependency is included because different objects might want to include
different files (imagine a large project with many subdirectories, each of
which has its own F<config.h>).

There's no C<file_in_object:> because there's no need to cache which paths
provide which files; it's obvious from their names.

=item C<symbol_required_by:>

A reference to a symbol, (as would be used by a linker), and the object that
requires it. Just as with C<file_required_by:>, this is used in dependencies,
and includes the name of the object with the dependency (for the same reason).

=item C<symbol_in_object:>

A reference to a symbol, and the object that contains it. This is used when
libraries and object files are specifying which symbols they provide. An
example would be C<symbol_in_object:exit:spath:/lib/libc.so>.

=item C<config_option:>

An option in the config file. This is mostly used in dependencies, to specify
things like "this object file needs to be rebuilt if the CFLAGS change" (a
build dependency), or "this source file has the install path substituted into
it at compile time" (a use dependency). The object is named using the option
name after C<config_option:>, e.g. C<config_option:CFLAGS>. These dependencies
are added automatically when options are substituted into commands.

=item C<config_rule:>

A rule in the config file. Every rule includes itself as a use
dependency. That way, if the config file is changed, files are recompiled
accordingly. Each rule has its own name for this purpose.

=item C<sys:>

Things like dummy targets, file-like objects that aren't files, and so on. The
following C<sys:> objects exist:

=over 4

=item C<sys:always_rebuild>

An object that conceptually changes with each run of aimake. This should only
be used for, e.g., "last build" reports in the About box of a program.
(B<aimake> also uses it internally to determine when to scan for new files in
the source tree, a task that should always be performed.)

=item C<sys:rebuild_late>

An object that conceptually depends on everything that doesn't depend on
it. This is typically used for wildcard-style build rules where you want to
depend on everything that obeys a particular pattern.

=item C<sys:touch_only>

An object that conceptually represents a file that is deleted immediately
after being installed. It can be installed in a directory to force the
directory to be created at install time.

=item C<sys:empty_file>

An object that conceptually represents a zero-length file. Its only use is to
be installed, creating or truncating an install target.

=item C<sys:ensure_exists>

An object that conceptually represents the file that already exists at an
install target. When installed, it will ensure that the install target exists
(by creating it as a zero-length file if necessary), but will not overwrite an
existing file at that location.

=item C<sys:broken>

An object that conceptually represents a file that cannot be generated. It's
added as a use dependency of an object when attempts to determine its
dependencies fail, to prevent it simply being used without its dependencies in
place.

=back

=item C<cmd:>

Represents a command shipped with the system, like C<gcc>. This is only
meaningful in the part of a configuration file that specifies what command to
run to accomplish something. (C<bpath:>, C<tool:> and C<intcmd:> objects can
also be used in the same context.) This is just a filename, no directories are
involved.

=item C<tool:>

A type of command shipped with the system, such as C<tool:c_compiler>. These
are provided by C<cmd:> objects, and are typically listed in use dependencies
of files that need compilation. (There's no build dependency version because
as tools are shipped with the system, which C compiler is used doesn't depend
on which C file is being compiled.)

=item C<intcmd:>

Represents a command that's emulated by aimake. This gives system-independent
behaviour for some commonly used commands:

=over 4

=item C<intcmd:nop>

An internal command that does nothing and produces no output. Typically used
to provide files that already exist.

=item C<intcmd:cat>

An internal command that just outputs the file given as its argument.

=item C<intcmd:writefile>

An internal command that takes a filename (as an C<optpath:> and a set of
contents for that file (as an C<optstring:>.) Any underscores in the contents
will be replaced with spaces, and any spaces with newlines (as usual for an
optstring), and then the resulting string will be written to that file (which
should be in the C<bpath:>). This is used to generate things like input files
for compiler tests.

=item C<intcmd:symlink>

An internal command that causes its second argument to become an alias for its
first argument. This will be done via a symbolic link if possible, but on some
platforms it may need to copy the file; thus, there is no guarantee that the
two arguments will have the same meaning forever. This will overwrite its
second argument if necessary. (On POSIXy systems, this is similar to B<ln
-sf>, except that it handles relative paths more intuitively.)

=item C<intcmd:testcase>

An internal command used for testing aimake. It prints its arguments to the
user, and errors out if the first argument contains "failed" anywhere.

=item C<intcmd:optionvalues>

An internal command that takes config options as its arguments (e.g.
C<optpath::config_option:packagename>), and outputs their values in
"name=value" form.

=item C<intcmd:listtree>

An internal command that takes a directory name as argument, and returns a
list of all the files (but not directories) recursively inside that directory,
one per line. (As such, it's similar to the POSIXy B<find>.) Any symbolic
links found are returned, but not dereferenced or followed. Unlike calling
commands external to aimake (whose output is typically split on newlines
before being fed to the regular expression specified in the config file), an
internal mechanism is used to remember where the boundaries of each filename
are, so this will work correctly even if, for some reason, your filenames
contain newlines, NULs, or other such bizarre characters.

If given an optstring as well as an optpath, the optstring specifies the
maximum nesting depth to go down the tree; 0 will just list the directory
itself, 1 will also list its subdirectories, and so on.

=back

=item C<optstring:>

An C<optstring:> object represents an option that needs to be provided to a
command. So for instance, the F<ar> command often used to make libraries might
have C<optstring:cr> as a use dependency. C<optstring:> objects are the first
thing to appear on a command line when a command is executed, and normally
start with an appropriate option character for the system (typically slash on
systems that don't use slashes as path separators, and hyphens
otherwise). These are specified as use dependencies, but become build
dependencies unchanged.

C<optstring:> objects are split on spaces, because typically the only use for
spaces inside an option is to specify a filename which contains embedded
spaces, and that would be done via C<optpath:>.

An C<optstring:> that has a leading space is moved to the end of the
command line, but otherwise works like the C<optstring:> without the
space.

=item C<optpath:>

A combination of an C<optstring:> and a file-like object. The format is, e.g.,
C<optpath:-o:bpath:file.h> if they should be combined into one command-line
option, and C<optpath:-o :bpath:file.h> (that's a space and a colon) if they
should be left as two consecutive command-line options.

=item C<optionset:>

A set of options commonly used together, such as the C<CFLAGS> commonly used
by B<make> programs (which would be C<optionset:CFLAGS>).

=item C<dependencies:>

The set of dependencies generated by a particular dependency rule,
e.g. C<dependencies:ch_file_dependencies:path:header.h> would represent the
dependencies of C<header.h> that were found via the rule
C<ch_file_dependencies>.

=back

=head1 CONFIGURATION

aimake is designed not to need configuration on simple projects, but
sufficiently complex projects may need some configuration to explain things to
aimake that it couldn't possibly guess (such as how to generate files that are
used as part of the build process).

Much of the workings of aimake are driven by a configuration file that's
embedded into the B<aimake> script itself, in order that only a single file
need be shipped. Any part of this script can be overriden by a configuration
file C<aimake2.config> in the root of the package source directory, via
specifying an option or a rule with the same name.

The format of the configuration file itself is Perl object notation; this
allows for comments, dictionaries, lists, numbers, strings, and regular
expressions:

    # this is a comment, from the # to the end of line
    4                          # a number
    'abcde'                    # a literal string
    "x\n\t$var"                # a string containing escapes, variables
    qr/foo.*bar/i              # a regular expression
    [1, 2, 3, 4,]              # a list, trailing comma optional
    {foo => 1, bar => 2,}      # a dictionary, trailing comma optional

The configuration file allows defining one or more variables (either to
abbreviate what would otherwise be repeated code, or to avoid "magic numbers"
in tthe code). There are also some predefined variables, which hold
platform-specific extensions: $exeext (executables), $objext (object files),
$libext (static libraries). These are followed by a single dictionary with two
keys, C<options> and C<rules>. Here's a simple example C<aimake2.config>:

    $version = "1.2.3";        # variable definition

    {
        options => {
            packagename => 'configexample',  # override an option
        },
        rules => {
            install_executable => undef,     # undefine a rule
            _generate_usage => {             # define a new rule
                command => ['extend:$exeext:bpath:gendocs', 'optstring:-u'],
                output => 'bpath:doc/usage',
            },
        },
    }

The options dictionary is very simple: keys of the dictionary are options, and
the corresponding values their values. The following options are recognised:

=over 4

=item packagename

A directory name that will be used to name any directories that are specific
to the package when installing.

=item ignore_directories

A regex of directory names to never consider part of the source tree. (By
default, this is a list of directories used to hold data by a range of version
control systems.)

=item ignore_directories_with_files

A list of filenames that cause directories containing them to never be
considered part of the source tree. (By default, this is a list of files
generated to hold internal state by autoconf, CMake, and aimake, which
indicate that the directory is being used as a build directory.)

=item ignore_directories_on_win32

A regex of directory names to ignore when running on Microsoft Windows.
(This allows for the common situation where client software is designed
to run on every OS, but server software is designed to run on POSIX OSes
only.)

=back

The rules dictionary is more complex. It consists of a number of rules; the
keys are the rule names, and the values the rules themselves. aimake's
predefined rules will all start with a lowercase letter. In order to avoid
clashes with them when defining your own rules, start the names of your own
rules with an underscore.

There are two sorts of rules: rules which specify dependencies of objects, and
rules which provice objects. Rules that produce an object have a C<command>
and C<output>, and perhaps also a C<object> if they need to be able to
generate filenames or the like, a C<verb> and/or a C<outputarg>, and may
specify C<propagate_usedeps>. Rules that provide an object work like
production rules, but also need a C<outdepends>.  Rules that specify
dependencies have an C<object> and a C<depends>, and perhaps also a C<command>
if commands need to be run to determine the dependencies, and/or a
C<dependsarg>. Either can have a C<inner>, C<prereq>, C<linesep>, C<lineskip>,
C<linemax>, C<unescape>, C<filter> and/or C<filter_spath>, and can specify
C<ignore_failure>, C<ignore_warnings>, and/or C<ignore_spath> if they have a
C<command>.

A rule itself is a dictionary with a number of keys:

=over 4

=item command

A list of objects, or a single object which will be interpreted as a 1-element
list. The list will be recursively extended with use dependencies of the
objects listed (and also the C<object>, if it exists, and if this is a
provicion rule). Once this has been done, all executable-like and option-like
objects will be used to form a command (either an internal command or one that
exists on the system, depending on which executable-like objects are
involved), and executed. (Alternatively, you get an error if you try to use
multiple different executables, or provide none.) The list of objects is also
considered a list of dependencies, to know when to rebuild.

If the command produces output on stdout, it is discarded unless C<output> is
a regexp. If it produces output on stderr, the user will be informed of it
(and there will be an indication of warnings in the progress report), but the
output will otherwise be ignored. If the command returns a failure status or
crashes with a signal, it will be reported as an error unless the
C<fail_silently> key is set, but regardless of whether the error is reported,
the rule will be treated as if it didn't exist.

For dependency rules, rather than provicion rules, C<command> is provided to
determine which command to use to determine the dependencies. It can be
omitted if it would just be C<intcmd:nop>. (It can also be omitted in
provicion rules if no command need be run for the rule; this happens in
provision rules quite a bit, and could also happen in rules that "generate"
C<spath:> objects.)

A null string in the C<command> of a dependency rule refers to the C<object>
(which must be given). This is, however, pointless, because the C<object> is
automatically included in the list if it exists for provicion rules, and would
lead to an infinite regress if included for dependency rules (because it would
specify calculating the command based on the dependencies of the object we're
trying to calculate dependencies for). Likewise, if just the "outside" part of
an object (e.g. C<optpath::>) is given, the C<object> is used to fill in the
"inside" (and this can be meaningful in dependency rules). Note that just
because the C<object> is automatically included in the dependency lists, this
doesn't necessarily mean it will be mentioned on the command line; use
C<optpath::> for that.

=item output

Just like the C<command>, a list of objects or a single object. You can use a
regular expression instead of an object name; that will cause the output of
the command to be parsed for lines matching the regular expression, and the
first parenthesized group matched by the regular expression will be treated as
a filename (for non-internal commands and C<intcmd:cat>), or an object name
(for C<intcmd:listtree>). (It's OK for the regular expression to match
multiple times on the same line, which will produce multiple output objects.)
Any carriage returns immediately before newlines in the output will be
ignored, for compatibility with programs using the wrong sort of newline
discipline for your platform. The list of output objects is considered to be
the objects that the rule is responsible for providing or producing, and they
must exist after the rule is executed. (It's OK if they existed beforehand; in
fact, there are standard rules that "generate" C<spath:> objects, with the
purpose of the rule being to inform aimake of the object's existence, and get
it to monitor for changes in it, rather than to create it.) If an object is
listed as providing or producing itself, that will be silently ignored
(because it's meaningless, and the config file is easier to write without
having to handle that case). Null strings, as usual, refer to the C<object>,
including nested inside other objects (if there's no C<inner>).

=item outdepends

Provision rules don't generate files on disk; they just say "objects X can be
used to provide object Y". In this case, object Y is the C<output>;
C<outdepends> specifies what objects X, the use dependencies of object Y are.
This is in exactly the same format as the C<output>, including the
substitution of the C<object> where required. C<outdepends> is omitted for
production rules, and for dependency rules.

=item propagate_usedeps

Sometimes, a provision rule will not need the normal use dependencies of an
object to be in place. (For instance, when looking at a file to see which
symbols it provides, you don't need access to the symbols that that file
uses.) Setting propagate_usedeps will cause the object itself to become a use
dependency of the objects it produces/provides, but not require those
dependencies to be satisfied to run the rule. (This is always the case for
dependency rules, so there's no point in specifying it.)

=item object

The object to specify dependencies of, in the case of a dependency rule. This
must be a single object. In the case of a provicion rule, the only purpose of
the C<object> is to determine what null strings mean in the C<command>,
C<output>, and C<outdepends>.

Instead of using an object name directly, you can use a regular expression;
this will cause the rule to be able to run once for each object that matches
(although it will only match one object at a time), in order to allow you to
write rules that, say, use a custom compiler you built to compile a custom
filetype (it'd be invoked on each file individually).

=item depends

The objects that that object depends on. These are "use dependencies";
anything that requires C<object> in its command must also require C<object>'s
C<depends>. This works exactly like C<output>. C<output> versus C<depends> is
what distinguishes a rule as a provicion rule versus a dependency rule.
(C<command> should be set if C<depends> is a regular expression, and can be
omitted otherwise.)

=item install_dir

An install rule is like a dependency rule, but it specifies an install
directory rather than a list of dependencies and thus specifies C<install_dir>
rather than C<depends>. If the C<-i> switch is used, the C<object> will be
installed into that directory.

The available install directories are as follows:

=over 4

=item bindir

A directory used for installing executables runnable by the user. This will
typically be somewhere that's on the $PATH by default.

=item libdir

A directory used for installing libraries that are used by tools provided with
the system, such as C compilers.

=item specificlibdir

A directory used for installing libraries that are only used by the program
being run.

=item configdir

A directory used for system-wide configuration files (that apply to all users
on a multi-user system).

=item datadir

A directory used for general read-only data files used by the program.

=item mandir

A directory used for documentation in UNIX manual format.

=item infodir

A directory used for documentation in GNU Info format.

=item statedir

A directory used for general data files that are both read and written by the
program (and that are shared between all users).

=item logdir

A directory used for log files that are written by the program.

=item lockdir

A directory used for files, shared between all users, that are transient in
nature. (The directory may be one where files are deleted on reboot.)

=item specificlockdir

A directory used for files, shared between all users, that are transient in
nature, that is not used by any other package (and thus, can be given files
with arbitrary filenames without worrying about name clashes). The directory
may be deleted on reboot, and as such, may have to be re-created each time it
is used.

=back

=item install_name

The basename to install a file under, if it isn't the same as the basename of
the C<object>.

=item inner

There are a few compound objects like C<searchpath:> and C<optpath:>. To
output such objects, C<output> or C<depends> can be set to just the "outside"
part of the object (e.g. C<searchpath:library:> or C<optpath:-o :>), and
C<inner> is used to specify the "inside" part of the object. It can be a
regular expression, with the same meaning as making C<output> or C<depends> a
regular expression. The default value of C<inner> is C<object>.

=item outputarg

Nested objects have the form C<objtype:outputarg:inner>. If C<output> is just
a C<objtype:outputarg:>, the C<inner> is used to fill in the rest of the
object name. If output is just a C<objtype:>, however, the C<outputarg> is
used to fill in the rest of the name; this works for both nested and
non-nested objects. (It's possible to have both an C<outputarg> and an
C<inner> in the case of a nested object.) C<outputarg> should be omitted if
not needed, and should be specified if needed. Just as with C<inner>, it can
be a regular expression; unlike with C<output> and C<inner>, it's interpreted
as a plain old string, not as a filename or object name. (Only one of
C<output>, C<inner>, and C<outputarg>, and C<outdepends> should be a regular
expression.)

=item dependsarg

Works exactly like C<outputarg>, except for C<depends> not C<output>.

=item in_subdir

By default, aimake uses absolute paths for command line arguments. Sometimes,
a program will only function correctly with relative paths. In such a case,
set C<in_subdir> to specify the current directory for running the command;
C<optpath:>s that refer to files within that subdirectory will be formatted as
relative paths.

=item filter

When C<output>, C<outdepends>, C<outputarg>, C<dependsarg>, C<depends>, or
C<inner> is a regular expression, C<filter> can also be a regular expression,
which tells aimake to disregard lines that don't match the C<filter> before
trying to match the other regular expression. Otherwise, it is ignored.

=item linesep

Changes what is considered to be a line separator in the output. You might,
for instance, want to split the output on spaces if trying to parse a Makefile
rule. The default is a newline.

If C<linesep> is combined with C<unescape>, then escaped literal newlines will
be deleted (as usual with C<unescape>, escaped C<linesep>s will be considered
to be a literal C<linesep>, non-escaped literal newlines will be considered to
be newline characters that don't break a line, and non-escaped C<linsep>s will
be considered a transition from one line to the next. (This makes it possible
for every codepoint to be used in the output from a program; as such,
C<aimake> will track the line separators internally.)

=item lineskip

After applying the C<filter> and C<linesep>, specifies the first line of
output that will be read, by specifying how many of the lines to skip at the
start. The default is 0.

=item linemax

After applying the C<filter>, C<linesep> and C<lineskip>, specifies the
maximum number of lines of output that will be read. If not provided (the
default), all the lines will be read.

=item unescape

Causes aimake to unescape the output from a program. By default, no unescaping
is done. At present, the only recognised values for C<unescape> are:

=over 4

=item C<'backslash'>

Backslash-newline is deleted; backslash followed by any letter in C<abfnrt>
will be replaced by a corresponding control character according to the rules
of C; and backslash followed by any other character will be replaced by that
character. (No vertical tabs, sorry. They're not portable between character
sets. At present, we don't implement decimal/hexadecimal/octal escapes,
either.)

=item C<'backslash_whitespace'>

Backslash-newline is deleted; backslash followed by other whitespace, or
another backslash, deletes the backslash but leaves the whitespace or second
backslash; and backslash followed by any other character preserves both the
backslash and the character. (This is the encoding gcc uses for makefile
fragments.)

=back

=item ignore_failure

If something goes wrong trying to run the C<command>, the entire rule will be
ignored for those inputs. Normally, the user will also be informed of this.
Setting C<ignore_failure> to 1 causes no messages to be printed upon failure;
this is typically used in situations where the easiest way to find out if a
command is applicable to a situation or not is to attempt to run it. The
rule's output will nonetheless not be used. Setting C<ignore_failure> to 2
causes failures to be treated as successes (e.g. if you're running a command
you suspect will fail to glean information from what it outputs before it
errors out.) It should be set to 1, 2 or omitted.

=item ignore_warnings

This is similar to C<ignore_failure>, but for successes; if the command
succeeds, the user will not be informed of any warnings produced running it.
It should be set to 1 or omitted.

=item ignore_spath

When a rule has a C<command>, C<ignore_spath> will cause any dependencies that
were only included due to C<spath:> objects to be ignored when working out the
command's arguments. The main purpose of this is to avoid statically linking
system libraries into libraries being built by B<aimake>.

=item filter_spath

Causes any C<spath:> objects that would be generated via a regular expression
match to be ignored if set to 1 (this is most common/useful in dependency
rules). It should be set to 1 or omitted.

=item filter_text_files

If set to 1, causes any objects that would be generated via a regular
expression match to be ignored if they appear to be text files, rather than
binary files (the main purpose of this is for filtering out linker scripts,
which cannot be distinguished from libraries via filename alone). It should be
set to 1 or omitted.

=item verb

C<verb> affects the message given (e.g. "Built file.o") when the rule succeeds
(and has no effect except on messages). The default is "built" for a provicion
rule. (Dependency rules cannot meaningly have a C<verb>, because the
dependencies are potentially determined via the cumulative effect of many
rules.)

=back

You can provide a list of dictionaries rather than a single dictionary as the
definition of a provicion rule. This allows for alternative versions of rules
to work using different compilers, or the like. aimake will pick the first
rule on the list which doesn't contain a nonexistent object in its C<command>,
nor its C<outdepends>. This can only be used for rules with no C<object> (it's
mostly intended for rules that provide C<tool:> objects). It can also only be
used for provicion rules, not for dependency rules.

=cut

use 5.008; # stay compatible with 5.8 for portability
use warnings;
use strict;

# Only modules that are shipped with Perl 5.8 may be listed here. (This is why
# we use Safe for configuration, rather than, say, YAML. Safe+eval lets us
# parse configuration files entirely in core.) Use Module::CoreList to check.
use Config;
use Carp;
use Storable qw/nstore retrieve/;
use Data::Dumper qw/Dumper/;
use File::Temp qw/tempfile/;
use File::Spec ();
use File::Path qw/make_path/;
use Cwd qw/cwd abs_path/;
use FindBin qw'$RealBin';
use ExtUtils::MakeMaker; # for the MM module
use Safe;
use File::Find qw/finddepth/;
use Memoize;
use Digest::MD5 qw/md5_hex/;
use Getopt::Std qw/getopts/;

use constant statefile => './aimake2.state';
use constant configfilename => 'aimake2.config';

my $cwd = cwd;

$Data::Dumper::Indent = 1;
$Data::Dumper::Sortkeys = 1;
$Data::Dumper::Useqq = 1;
$Data::Dumper::Terse = 1;
$Data::Dumper::Deepcopy = 1;
$Data::Dumper::Quotekeys = 0;

$| = 1;

# $state contains information that persists between runs. There are some keys
# used for special purposes, like _srcdir, but mostly this consists of keys
# which are object names. The value corresponding to an object name is a
# dictionary with the following keys:

# generated: a map that specifies the rule that was used to generate that
# object (_rule), and the object it was built from (_object). The object is
# up-to-date if nothing has changed that would affect the operation of that
# rule (this definition is recursive, in that determining whether the
# operation of the rule is affected depends on which objects are
# up-to-date). This key is required for all objects to exist, apart from
# objects which "always exist" (sys:, optstring:, cmd:, etc.). There's also a
# _use_deps key that contains the outdepends that were generated at the same
# time as the object was provided, for provided objects.
# depends: a map from rule names to generation histories, as well as a
# _use_deps key that contains the depends that were generated by running that
# rule. Whenever an object's use dependencies need calculating, a search is
# made for rules that permit that object; rules are run if they don't exist in
# provided, rerun if they exist but with the wrong hashes, and deleted from
# provided if they don't exist any more or apply to the wrong sort of
# object. The object itself is always included in the list of object hashes in
# depends, if it's file-like (and never is in generated).
# last_attempt: a map from rule names to generation histories, that specifies
# the dependencies the last time that rule was run with that object. (To
# avoid running rules twice in a row if nothing has changed.) This is only
# used for provicion rules ("depends" handles dependency rules).
my $state = {};

my %filecache = (); # hash from OS filenames to file contents

# In order to detect config file mistakes (that would otherwise cause infinite
# loops, we take note whenever a rule outputs something on the bpath. If
# multiple rules alter the same bpath element, or if the hash of a bpath
# element changes behind our back, we complain.
my %bpathhash = ();
my %bpathgenerator = ();

# Check for a statefile in the current directory.
if (-e statefile) {
    # throws exception if there are serious errors
    $! = 0;
    $state = retrieve(statefile);
    # but minor errors return undef
    defined $state or die "Error reading statefile: $!";
    # Ensure the statefile exists, so that we don't recurse into our own
    # directory.
    nstore($state, statefile);
}

# Parse arguments.
our $VERSION = 2.0;
$Getopt::Std::STANDARD_HELP_VERSION = 1;
my %options;
getopts('drv:i:p:B:W:', \%options);

if($options{'d'}) {
    print Dumper($state);
    exit 0
}

defined($options{'v'}) or $options{'v'} = 1;
!defined $options{'v'} or $options{'v'} =~ /^-?\d+$/
    or die "Argument to -v must be a number";

if (scalar @ARGV == 1) {
    defined $state->{_srcdir} and
        $state->{_srcdir} ne absolute_path($ARGV[0]) and
        die "This build directory is already being used to build '".
        $state->{_srcdir}."' (you wanted to build '".absolute_path($ARGV[0])."')";
    if (!defined $state->{_srcdir}) {
        # Start a new build. We first want to verify that the
        # directory is empty.
        opendir my $dh, '.' or die "Cannot open '.': $!";
        my @offending_files = File::Spec->no_upwards(readdir($dh));
        closedir $dh;
        @offending_files and die
            "Please use an empty directory for starting new builds.";
        # Should be OK to start, then.
        $state->{_srcdir} = $ARGV[0];
    }
} elsif (scalar @ARGV > 1) {
    die "Don't know what to do with multiple non-option arguments.";
}

if (!defined $state->{_srcdir}) {
    # Determine the source directory by looking at where this script is.
    # TODO: Check to see if we're some sort of globally-installed aimake,
    # rather than shipped with a distribution, and error out.
    $state->{_srcdir} = $RealBin;
}

$state->{_srcdir} = absolute_path($state->{_srcdir});

$state->{_runcount}++; # so always_rebuild has a different hash each time

# Save the statefile upon exit.
# TODO: For this to be correct, we need to be careful in the order we
# update things that are in the statefile.
sub sighandler {
    die @_ if $^S;
    progress_report(4, "Exiting, writing statefile...");
    progress_report(0, ""); # clear any partial messages from earlier
    nstore($state, statefile)
        or die "Couldn't store state file: $!";
    @_ and confess @_;
    exit $?;
}
$SIG{INT} = \&sighandler;
$SIG{__DIE__} = \&sighandler;

# We use Perl notation (Data::Dumper-style) for the config file, which leads
# to a bit of a problem reading it. The solution: use Perl's parser, and the
# Safe module to allow only specification of constants. (rv2sv is there so
# that variables can be used to avoid repetition.)
my $configsafe = new Safe;
$configsafe->permit_only(qw/null stub scalar pushmark const undef
                            list qr negate lineseq leaveeval anonlist
                            anonhash rv2sv sassign nextstate padany
                            regcreset concat stringify quotemeta/);
# These are global, not lexical, so we can share them with the config file
our $libext = $Config{_a};
our $objext = $Config{_o};
our $exeext = $Config{_exe};
$configsafe->share('$libext', '$objext', '$exeext');

$! = 0;
my $config;
my $configdata;
{
    local $/;
    $configdata = <DATA>;
}
$config = $configsafe->reval($configdata, 0);
$@ or ($! and $@ = $!);
$@ or $@ = "bad configuration file format";
defined $config or
    die "Could not load global config file: $@";
undef $@;
ref $config eq 'HASH' or die "Invalid type in global config file";

my $localconfigfile = File::Spec->catfile($state->{_srcdir}, configfilename);
if (-f $localconfigfile) {
    local $/;
    open my $fh, '<', $localconfigfile
        or die "Could not open config file: $!";
    $configdata = <$fh>;
} else { $configdata = '{}'; }
my $localconfig = $configsafe->reval($configdata, 0);
$@ or ($! and $@ = $!);
$@ or $@ = "bad configuration file format";
defined $localconfig or
    die "Could not load project-specific config file: $@";
undef $@;
ref $localconfig eq 'HASH'
    or die "Invalid type in project-specific config file";
for my $t (qw/rules options/) {
    if (exists $localconfig->{$t}) {
        $config->{$t}->{$_ =~ /^_/ ? "project$_" : $_} =
            $localconfig->{$t}->{$_}
            for keys %{$localconfig->{$t}};
    }
}

# Directory options need substitutions, and to be made from aimake
# paths relative to -i into OS paths.
for my $option (keys $config->{options}) {
    if ($option =~ /dir$/) {
        1 while $config->{options}->{$option} =~
            s/\$([a-z]+)/$config->{options}->{$1}/g;
    }
}

defined($options{'i'}) or $options{'i'} = aimake2os('aimake_install', $cwd);
-d $options{'i'} or make_path $options{'i'};
$options{'i'} = absolute_path($options{'i'});

# Produce a sensible package name.
{
    my ($vol, $dirs, $file) = File::Spec->splitpath($state->{_srcdir});
    if (!defined $file || $file eq '') {
        my @dirs = grep {$_ ne ''} File::Spec->splitdir($dirs);
        $file = $dirs[(scalar @dirs) - 1];
    }
    defined($config->{options}->{packagename}) or
        $config->{options}->{packagename} = $file;
}

my $install_path_aimake = os2aimake($options{'i'});

for my $option (keys $config->{options}) {
    if ($option =~ /dir$/) {
        $config->{options}->{$option} = object(
            "$install_path_aimake/" . $config->{options}->{$option}, 'stringify');
        progress_report(4, "Install path: $option = " . $config->{options}->{$option});
    }
}

# Prepare the caches of which rules can apply to which objects. (We're
# talking about the "object" field of the rules, here.)
my %applicable = ();
for my $objname (keys %$state) {
    next if $objname =~ /^_/;
    update_applicable($objname);
}
for my $t (qw/rule option/) {
    update_applicable("config_$t:$_")
        for keys %{$config->{"${t}s"}};
}

# Some information about object classes.

# File-like objects: those that can meaningfully be rebuilt and/or
# changed outside aimake.
my %filelike = (path => 1,
                bpath => 1,
                spath => 1,
                searchpath => 0, # is notional, like "file"
                extend => 0,
                file_required_by => 0,
                symbol_required_by => 0,
                symbol_in_object => 0,
                config_option => 1, # read from config file
                config_rule => 1, # ditto
                sys => 1, # nonfiles that act like files
                install_target => 0,
                cmd => 1,
                tool => 0,
                intcmd => 1, # acts like cmd
                optstring => 0, # these are immutable
                optpath => 0, # "provided" by the mentioned object
                optionset => 0,
    );

# Nested objects: objects that take other objects as arguments.
my %nested = (path => 0,
              bpath => 0,
              spath => 0,
              searchpath => 1,
              extend => 1,
              file_required_by => 1,
              symbol_required_by => 1,
              symbol_in_object => 1,
              config_option => 0,
              config_rule => 0,
              sys => 0,
              install_target => 1,
              cmd => 0,
              tool => 0,
              intcmd => 0,
              optstring => 0,
              optpath => 1,
              optionset => 0,
    );

# Lists hashes for sys:, implementations for intcmd:
# Every value here must evaluate to true.
# An implementation of an internal command returns
# (failreason, stdout, stderr).
my %intobject = (
    "sys:always_rebuild" => $state->{_runcount},
    "sys:rebuild_late" => $state->{_runcount},
    "sys:touch_only" => 1,
    "sys:empty_file" => 1,
    "sys:ensure_exists" => 1,
    "intcmd:nop" => sub {return (undef, "", undef)},
    "intcmd:cat" => sub {
        $_[0] or return ("Not enough arguments", '', undef);
        update_file_cache($_[0], 1) or return
            ("Could not read '$_[0]'", '', undef);
        my $file = $filecache{$_[0]};
        $file =~ s/\r$//gm;
        return (undef, $file, undef);
    },
    "intcmd:writefile" => sub {
        my $fn = pop @_;
        s/_/ /g for @_;
        open my $fh, '>:raw', $fn or
            return ("$fn: $!", '', undef);
        print $fh "$_\n" for @_;
        close $fh;
        return (undef, '', undef);
    },
    "intcmd:symlink" => sub {die "TODO"},
    "intcmd:testcase" => sub {
        for (@_) {
            progress_report(0, "intcmd:testcase argument: $_");
        }
        $_[0] =~ /failed/ and
            return ('First argument contained "failed"', '', undef);
        return (undef, '', undef);
    },
    "intcmd:optionvalues" => sub {
        $_[0] or return ("Not enough arguments", '', undef);
        my $stderr = '';
        my @stdout = ();
        my $failreason = undef;
        while (@_) {
            if (defined $config->{options}->{$_[0]}) {
                push @stdout, $_[0] . "=" . $config->{options}->{$_[0]};
            } else {
                $failreason = "Some config options did not exist";
                $stderr .= "optionvalues: $_[0] does not exist\n";
            }
            shift;
        }
        return ($failreason, \@stdout, $stderr);
    },
    "intcmd:listtree" => sub {
        my @results;
        my $starttime = time;
        my $lastdir = '';
        my $depth = undef;
        scalar @_ > 1 and $depth = shift;
        my $dir = shift;
        $dir or return ("Not enough arguments", '', undef);
        -d $dir or return ("$dir is not a directory", '', undef);
        my $in_spath = os2aimake($dir) =~ /^spath:/;
        finddepth({
            no_chdir => 1,
            wanted => sub {
                push @results, os2aimake($File::Find::name);
                # This can be a quick operation. It can also be very slow.
                # If it's taken more than between 1 and 2 seconds, start
                # showing a progress bar.
                if ((time > $starttime + 1 || $options{v} > 1) &&
                    $lastdir ne $File::Find::dir) {
                    $lastdir = $File::Find::dir;
                    progress_report(2.5, "Looking in $lastdir");
                }
            },
            # We need to skip things like VCS directories. We don't
            # bother checking for them in the spath in order to speed
            # things up, because they're unlikely to be there.
            preprocess => sub {
                my (undef, $nested) = ospath_inside($File::Find::dir, $dir);
                return if defined $depth && $nested > $depth;
                return @_ if $in_spath;
                return grep {
                    $_ !~ $config->{options}->{ignore_directories} &&
                        (
                         $^O ne 'MSWin32' ||
                         !$config->{options}->{ignore_directories_on_win32} ||
                         $_ !~ $config->{options}->{ignore_directories_on_win32}
                        ) and do {
                        my $ok = 1;
                        -e File::Spec->catfile($File::Find::dir, $_)
                            and $ok = 0
                            for @{$config->{options}->
                                  {ignore_directories_with_files}};
                        $ok;
                    }
                } @_;
            }
        }, $dir);
        return (undef, [@results], undef);
    },
    );

if ($options{'W'}) { goto DISPLAY_ERRORS; }

# Main loop.
sub progress_report;
my $anychanges = 1;
my $rebuild_late_lock = 1;
progress_report 2.5, "Looking for rules to apply...";
while ($anychanges) {
    $anychanges = 0;
    # We keep looking for provicion rules that have only up-to-date
    # expanded dependencies, and executing them. (Dependency rules are
    # executed when calculating expanded dependencies.)
    for my $rulename (sort keys %{$config->{rules}}) {
        my $rule = $config->{rules}->{$rulename};
        # The rule could be an array ref, not a hash ref. If so, search for
        # the first entry with only existing objects in its command and
        # outdepends.
        if (ref $rule eq 'ARRAY') {
            IRULE: for my $irule (@$rule) {
                my @needed = (@{defined_or($irule->{'command'},[])},
                              @{defined_or($irule->{'outdepends'},[])});
                object($_) or (progress_report 4, "$_ blocks $rulename."),
                    next IRULE for @needed;
                $rule = $irule;
                goto FOUND_IRULE;
            }
            progress_report 4, "No viable option found for $rulename.";
            next; # no rules applied
        }
      FOUND_IRULE:
        die "Rule $rulename has no output, depends, or install directory"
            unless exists $rule->{output} || exists $rule->{depends} ||
                   exists $rule->{install_dir};
        progress_report 4, "Checking rule $rulename...";
        my @objects = ('sys:no_object');
        defined $rule->{object} and
            @objects = sort keys %{$applicable{"_$rulename"}};
        for my $object (@objects) {
            # We have a rule, and an object it could apply to (if it
            # applies to an object at all).
            my ($runnable, $usedeps) = rule_runnable($rulename, $object);
            $runnable == 1 and $rebuild_late_lock
                and grep $_ eq'sys:rebuild_late', @$usedeps
                and next; # can't rebuild it yet
            $runnable > 0 and
                delete $state->{_brokendeps}->{"$rulename $object"};
            $runnable == 1 or next; # can run, and will do something

            # Not only does the user want to run the rule, but it's actually
            # possible to run it.
            my $shortobject = shorten_object($object);
            my $level;
            if ($rule->{install_dir}) {
                progress_report 1, "Installing $shortobject (rule $rulename)...";
                $level = 1;
            } elsif ($rule->{depends}) {
                progress_report 2,
                "Finding dependencies of $shortobject (rule $rulename)...";
                $level = 2;
            } elsif ($rule->{outdepends}) {
                if ($object eq 'sys:no_object') {
                    progress_report 2, "Finding objects (rule $rulename)...";
                    $level = 2;
                } else {
                    progress_report 2,
                    "Finding contents of $shortobject (rule $rulename)...";
                    $level = 2;
                }
            } elsif ($object eq 'sys:no_object') {
                progress_report 1, "Running build rule $rulename...";
                $level = 1;
            } else {
                progress_report 1,
                "Building from $shortobject (rule $rulename)...";
                $level = 1;
            }
            my $output_target = defined_or($rule->{output},
                                           "dependencies:$rulename:$object");
            if ($rule->{install_dir}) {
                $object =~ m=[/:]((?:[^/:\\]|\\[\\/:])+)$=;
                $output_target = os2aimake(
                    $config->{options}->{$rule->{install_dir}}) . "/" .
                    defined_or($rule->{install_name}, $1);                               
            }
            # If the output is a regexp, then (obviously) the command run
            # is responsible for creating the directory it's in.
            # If it isn't a regexp, then we're responsible for creating
            # the directory it's in.
            my ($outputstr) = replace_extend($output_target);
            $outputstr =~ /^bpath:(.*)/ and 
                ensure_directory(aimake2os($1, $cwd));
            my $verb = defined_or($rule->{verb}, 'built');
            # If we're installing, then do the file copy now.
            my $forcefail = undef;
            if (defined $rule->{install_dir}) {
                $verb = "installed";
                if ($object !~ /^sys:/) {
                    ensure_directory(object($output_target, 'stringify'));
                    update_file_cache(object($object, 'stringify'), 1);
                    if (open my $fh, '>:raw', object($output_target, 'stringify')) {
                        $fh->print($filecache{object($object, 'stringify')});
                        close $fh;
                        -x object($object, 'stringify') and
                            chmod 0755, object($output_target, 'stringify');
                    } else { $forcefail = $!; }
                }
                # else TODO: special sorts of install
            }
            my ($failed, $output, $outdepends, $stderr) = run_rule_command(
                $usedeps,               # TODO: ignore_spath:
                $rule->{filter}, $output_target,
                $rule->{outputarg},
                defined_or($rule->{inner}, $object),
                defined_or($rule->{outdepends}, $rule->{depends}),
                $rule->{dependsarg},
                $rule->{filter_spath},
                $rule->{filter_text_files},
                $rule->{linesep}, $rule->{unescape},
                $rule->{lineskip}, $rule->{linemax},
                $rule->{in_subdir});
            $forcefail and $failed = $forcefail;
            $rule->{ignore_failure} and
                $rule->{ignore_failure} == 2 and $failed = undef;
            $rule->{ignore_warnings} and !$failed and $stderr = undef;
            $rule->{propagate_usedeps} and push @$outdepends, $object;
            my $msg = $failed;
            $output = [grep {$_ ne $object} @$output];
            my @shortened = map {shorten_object($_)} @$output;
            if (@shortened && $shortened[0] =~ /^dependencies:/) {
                @shortened = map {shorten_object($_)} @$outdepends;
                $verb = "found";
            }
            if (!$failed) {
                if (scalar @shortened == 0) {
                    $msg = "Succeeded but $verb no objects";
                } elsif (scalar @shortened <= 4) {
                    $msg = ucfirst $verb . " " . join ', ', @shortened;
                } else {
                    $msg = ucfirst $verb . " " . $shortened[0] . ", " .
                        $shortened[1] . ", ". $shortened[2] . ", and " .
                        (scalar @shortened - 3) . " other objects";
                }
                $msg .= (defined $stderr && $stderr ne '' ?
                         " (with warnings)." : ".");
            } else {
                if ($object eq 'sys:no_object') {
                    $msg = "Couldn't run rule $rulename: $msg";
                } else {
                    $msg = "Couldn't run rule $rulename on $object: $msg";
                }
            }
            progress_report +($failed ? 0 : $level), $msg;
            # If it fails, it outputs nothing.
            $failed and $output = [];
            my @bpathoutput = grep {/^_bpath:/} @$output;
            # object_created must be called before hashes_from_command.
            object_created($_) for @$output;
            my %hfc = hashes_from_command([@$usedeps,"config_rule:$rulename"],
                                          $output);
            $hfc{$_} and die "Output $_ was used to generate itself"
                for @$output;
            $state->{$_}->{generated} = {_rule => $rulename,
                                         _object => $object} for @$output;
            defined $bpathgenerator{$_} and
                $bpathgenerator{$_} ne "$rulename:$object" and die
                "Multiple rules are trying to generate $_: $rulename:$object, ".
                $bpathgenerator{$_} for grep /^bpath:/, @$output;
            $bpathgenerator{$_} = "$rulename:$object" for @bpathoutput;
            $bpathhash{$_} = $hfc{_outputs}->{$_} for @bpathoutput;
            # Calculating which outputs are on the bpath actually takes a
            # surprising amount of time. So memoize it.
            $hfc{_bpathoutputs} = [@bpathoutput];
            $rule->{outdepends} || $rule->{depends} and
                $state->{$_}->{generated}->{_use_deps} = [@$outdepends]
                for @$output;
            $state->{$object}->{last_attempt}->{$rulename} = {%hfc};
            defined $stderr and $stderr ne '' and
                $state->{$object}->{last_attempt}->{$rulename}->{_warnings} =
                $stderr;
            if ($failed) {
                $state->{$object}->{last_attempt}->{$rulename}->{_failreason} =
                    $failed;
                $state->{$object}->{last_attempt}->{$rulename}->{_command} =
                    $usedeps;
                $state->{$object}->{last_attempt}->{$rulename}->
                    {_ignore_failure} = defined_or($rule->{ignore_failure}, 0);
            }
            $anychanges = 1;
        }
    }
    if (!$anychanges && $rebuild_late_lock) {
        $rebuild_late_lock = 0;
        $anychanges = 1;
    }
}


# Delete all objects that aren't up to date.
progress_report 2.5, "Deleting outdated objects...";
for my $object (keys %$state) {
    $object =~ /^_/ and next;
    up_to_date($object) or delete_object($object);
}

progress_report(0, ""); # clear any partial messages from earlier

# Display any warnings or errors produced during the build.

# Possible places to find errors:
# $state->{}->{last_attempt}->{}->{_failreason}
#   Error returned by a command when running a build rule.
#   Stderr is in {_warnings}; the command is in {_command}.
#   The error should not be displayed if {_ignore_failure}.
# $state->{}->{depends}->{}->{_failreason}
#   Error returned by a command when running a dependency rule.
#   {_warnings}, {_command}, {_ignore_failure} have the same meaning.
# $state->{_brokendeps}->{}
#   Rule could not be run because some of its implied dependencies were
#   missing. The missing dependencies are available as the value. If
#   sys:broken is on the list, this was caused by a failure to determine the
#   dependencies.
DISPLAY_ERRORS: ;
# First, we display problems running commands.
for my $object (keys %$state) {
    $object =~ /^_/ and next;
    defined $options{'W'} and $object !~ /$options{'W'}/ and next;
    if (exists $state->{$object}->{last_attempt}) {
        my $rules = $state->{$object}->{last_attempt};
        for my $rulename (keys %$rules) {
            my $dependencies =
                ref $config->{rules}->{$rulename} eq 'ARRAY' ?
                defined $config->{rules}->{$rulename}->[0]->{depends} :
                defined $config->{rules}->{$rulename}->{depends};
            if (defined $rules->{$rulename}->{_failreason} &&
                !$rules->{$rulename}->{_ignore_failure}) {
                my $msg = "Could not " .
                    ($dependencies ?
                     "determine dependencies of " : "build from ") .
                     shorten_object($object) . " (rule $rulename): ";
                $object eq 'sys:no_object' and
                    $msg = "Could not run rule $rulename: ";
                progress_report(0, $msg . 
                                $rules->{$rulename}->{_failreason});
                progress_report(0, "Failing command was " .
                                run_command(@{$rules->{$rulename}->{_command}},
                                            \ 'dontrun'));
                if ($rules->{$rulename}->{_warnings}) {
                    progress_report(0, "The command's error messages were:");
                    progress_report(0, $rules->{$rulename}->{_warnings});
                }
            }
            if (!defined $rules->{$rulename}->{_failreason} &&
                defined $rules->{$rulename}->{_warnings}) {
                my $msg = ($dependencies ?
                           "Determining dependencies of " : "Building from ") .
                           shorten_object($object). " with rule $rulename " .
                           "produced warnings:";
                progress_report(0, $msg);
                progress_report(0, $rules->{$rulename}->{_warnings});
            }
        }
    }
}
# Then, we display dependency errors.
# This is one of those situations where one cause can have hundreds of
# effects, so we display the causes and a summary of the effects. This
# basically means summarizing and reversing the _brokendeps hash.
if ($state->{_brokendeps} && keys %{$state->{_brokendeps}}) {
    my %causes;
    for my $effect (keys %{$state->{_brokendeps}}) {
        defined $options{'W'} and $effect !~ /$options{'W'}/ and next;
        my @causes = map {shorten_object($_)}
            @{$state->{_brokendeps}->{$effect}};
        $effect =~ /^[^ ]+ (.*)$/s or die "Invalid effect format $effect";
        my $shorteffect = shorten_object($1);
        $causes{$_}->{$shorteffect} = 1 for @causes;
    }
    progress_report(0, "The following dependencies were not found:");
    for my $cause (sort keys %causes) {
        next if $cause =~ /^dependencies:/; # already reported
        my $objects;
        my @effects = sort keys $causes{$cause};
        if (scalar @effects > 4) {
            $objects = $effects[0] . ", " . $effects[1] . ", " .
                $effects[2] . ", and " . (scalar @effects - 3) .
                " other objects";
        } else {
            $objects = join ', ', @effects;
        }
        progress_report(0, $cause . "   (needed to build from $objects)");
    }
}

# Save statefile on exit.
$? = 0;
sighandler();
exit 0; # unreachable

# Subroutines.

# Perl version independence.
sub defined_or {
    my $a = shift;
    return (defined($a) ? $a : shift);
}

# Object handling.

# The main function object() retrieves an object, or information about
# it. With one argument, it just returns true or false, for whether the object
# exists or not. With a second argument, it's interpreted as the field of the
# object to return. This works like getter methods in object-oriented
# programs; it might just retrieve a property stored somewhere, but might be
# calculated on the fly. There can also be a third argument, which is a
# disambiguator; this is necessary in cases like e.g. object("path:src/foo.c",
# "use_deps", "config_rule:relocatable_object"), because the use dependencies
# of a C file depend on how it's being required (in this case, one of the use
# deps would be "optstring:-fpic", which isn't required for building a normal
# sort of object).

# Here are some of the fields that are common to many sorts of object:
# exec: A subroutine reference (most likely a closure) that's used to execute
# the object like a command, existing for bpath:, cmd:, tool:, intcmd:.
# stringify: The way an object appears on a command line. For file-like
# objects, this will be their filename (absolute and in local OS format); for
# optstring: objects, this will be the optstring itself. This can return
# multiple return values in the case of optpath: objects.
# stringify-relative: Like stringify, but produces relative paths.
# hash: For path:, config_option:, config_rule: objects, the hash of the
# object (used to determine if these have changed between runs of aimake). The
# md5 hash of the file contents is used for path:; the md5 hash of the Dumper
# of the rule for a config rule or config option; things like sys: objects
# have their own hash values that change when necessary. This returns live
# information (although it won't necessarily cause a disk read each time; if
# the modification time hasn't changed since the last time the file's hash was
# calculated, the hash will be assumed to have not changed either).
# backed: Returns true if the object is file-like and there is a file or
# similar persistent state (e.g. in the config file) underlying it, false
# otherwise. This can return true even if the object doesn't "exist" (because
# it isn't known to aimake).

sub object {
    my $objname = shift;
    my $field = shift;
    my $configrule = shift;

    $objname =~ /^([^:]+):(.*)$/s or die "Invalid object name $objname";
    my $objtype = $1;
    my $objvalue = $2;
    my $objinner = undef;
    if ($nested{$objtype}) {
        $objvalue =~ /^([^:]*):(.*)$/s or die "Invalid subobject name $objvalue";
        $objvalue = $1;
        $objinner = $2;
    }

    # This checks to see whether the object exists on the filesystem /and/ has
    # been generated via a suitable method for aimake /and/ is up to date.
    if (!defined $field) { # existence check
        # Things that always exist
        $objtype eq 'config_option' and
            return exists $config->{options}->{$objvalue};
        $objtype eq 'config_rule' and
            return exists $config->{rules}->{$objvalue};
        $objtype eq 'cmd' and return locate_command($objvalue);
        $objtype eq 'sys' || $objtype eq 'intcmd'
            and return $intobject{$objname};
        $objtype eq 'optstring' and return 1;
        # It's OK for an optpath to refer to an object that doesn't exist,
        # because it might be an option to say where to create a file.
        $objtype eq 'optpath' and return 1;

        # Things that need resolution to another object: *_required_by, extend
        # At the moment we just let up_to_date return false on these; extend
        # should only exist in rules (and be replaced if it tries to escape
        # them), and *_required_by should be resolved by
        # expand_use_dependencies and thus not reach the existence check.

        # Things that need to be known to aimake (i.e. some rule produces or
        # provides them)
        return unless up_to_date($objname);

        # Objects on the filesystem must correspond to existing files
        $objtype =~ /^(?:path|bpath|spath)$/ and
            return (-e object($objname, 'stringify'));
        return 1;
    }

    if ($field eq 'stringify-relative') {
        my $absname = undef;
        $objtype eq "path"
            and $absname = aimake2os($objvalue, $state->{_srcdir});
        # $cwd can differ from the actual current working directory while we're
        # here. $cwd is the correct one to use, because the actual build path
        # hasn't moved on the filesystem.
        $objtype eq "bpath" and $absname = aimake2os($objvalue, $cwd);
        $objtype eq "spath" and $absname = aimake2oss($objvalue);
        $objtype eq "searchpath" and $absname = object($objinner, 'stringify');
        defined $absname and return File::Spec->abs2rel($absname);
    }
    # return OS path or option text
    if ($field eq 'stringify' || $field eq 'stringify-relative') {
        $objtype eq "path"
            and return aimake2os($objvalue, $state->{_srcdir});
        $objtype eq "bpath" and return aimake2os($objvalue, $cwd);
        $objtype eq "spath" and return aimake2oss($objvalue);
        $objtype eq "searchpath" and return object($objinner, 'stringify');
        $objtype eq "cmd" and return locate_command($objvalue);
        # Embedded spaces in optstrings mean separate options, not one
        # option that contains spaces. (Embedded spaces in opt/path/s mean
        # embedded spaces, in case someone put those in their filename.)
        $objtype eq "optstring" and $objvalue =~ /^ (.*)$/ and
            return split ' ', $1;
        $objtype eq "optstring" and return split ' ', $objvalue;
        if ($objtype eq "optpath") {
            # Note that we always use an absolute path here. This is so that
            # malicious or bizarre filenames don't get misinterpreted as
            # options. (e.g. a file called "-rf" would become, say,
            # /home/user/-rf, which "rm" interprets correctly.)
            $objvalue =~ s/ $//
                and return ($objvalue, object($objinner, $field));
            return $objvalue . object($objinner, $field);
        }
        $objtype eq "config_option" and return $objvalue; # for optionvalues
        die "Attempted to stringify unstringifiable object $objname";
    }

    if ($field eq 'exec') { # return closure for executing an object
        my $execmd;
        $objtype =~ /^(bpath|cmd)$/ and
            $execmd = object($objname, "stringify"),
            return sub {
            # Because all strings are potential arguments, arguments are given
            # to us as scalar references at the end of @_.
            my %args;
            while (ref $_[-1]) {
                $args{${pop @_}} = 1;
            }
            progress_report 3, "Running $execmd ".join(' ', @_);
            # We want to run the command, and capture its stdout and stderr.
            # We also want to avoid shell quoting mishaps, so we can't use the
            # versions of system or open that do redirection for us. And we
            # also want OS independence, so we can't create pipes, or use
            # Open2 or the like, etc.
            # So we temporarily redirect stdout and stderr to files, then read
            # back the files. This should work on pretty much any OS, and the
            # ones where it doesn't, we have no hope of reasonably being able
            # to function correctly.
            open my $save_stdout, ">&", \*STDOUT or die "Could not save stdout";
            my $tmp_stdout = tempfile() or die "Could not create temp file";
            open STDOUT, ">&", $tmp_stdout or die "Could not capture stdout";
            my ($save_stderr, $tmp_stderr);
            unless ($args{nostderr}) {
                open $save_stderr, ">&", \*STDERR or die "Could not save stderr";
                $tmp_stderr = tempfile() or die "Could not create temp file";
                open STDERR, ">&", $tmp_stderr or die "Could not capture stderr";
            }
            # Repeating $execmd like this forces shells to never be
            # involved. This works better than trying to understand Windows'
            # shell escaping rules.
            my $sysreturn = system { $execmd } $execmd, @_;
            open STDOUT, ">&", $save_stdout or die "Could not restore stdout";
            seek $tmp_stdout, 0, 0;
            unless ($args{nostderr}) {
                open STDERR, ">&", $save_stderr or die "Could not restore stderr";
                seek $tmp_stderr, 0, 0;
            }
            my ($stdout, $stderr);
            {
                local $/;
                $stdout = <$tmp_stdout>;
                $stderr = <$tmp_stderr> unless $args{nostderr};
                $stdout =~ s/\r$//gm;
                $stderr =~ s/\r$//gm unless $args{nostderr};
            }
            close $tmp_stdout;
            close $tmp_stderr unless $args{nostderr};
            my $failreason = undef;
            if ($sysreturn == -1) {$failreason = "Could not execute command";}
            elsif ($sysreturn >> 8 != 0) {
                $failreason = "Command exited with failure status " .
                    ($sysreturn >> 8);
            } elsif ($sysreturn != 0) {
                $failreason = "Command crashed with signal " .
                    ($sysreturn & 127);
            }
            progress_report 3, (defined_or($failreason, "Command succeeded").".");
            return ($failreason, $stdout, $stderr);
        };
        $objtype eq "intcmd" and return ($intobject{$objname} ||
                                         die "No such object $objname");
        die "Attempted to execute nonexecutable object $objname";
    }

    if ($field eq 'backed') {
        $objtype =~ /^[bs]?path$/ and return (-e object($objname, 'stringify'));
        $objtype =~ /^cmd$/ and return object($objname, 'stringify');
        $objtype eq 'config_option'
            and return exists $config->{options}->{$objvalue};
        $objtype eq 'config_rule'
            and return exists $config->{rules}->{$objvalue};
        $objtype eq 'intcmd' || $objtype eq 'sys'
            and return $intobject{$objname};
        return 0;
    }

    if ($field eq 'hash') {
        # This is only defined for file-like objects, but is defined for all
        # file-like objects, and changes whenever the object changes.
        # The file-like objects that don't have underlying files are
        # config_option, config_rule, sys, and intcmd.
        return 'x' unless object($objname, 'backed');
        $objtype eq 'cmd' and return command_version($objname);
        $objtype eq 'config_option' and return config_hash('options',$objvalue);
        $objtype eq 'config_rule'   and return config_hash('rules',  $objvalue);
        $objtype eq 'intcmd' and return $objvalue; # intcmds don't change
        $objtype eq 'sys' and return $intobject{$objname};
        my $filename = object($objname, 'stringify');
        update_file_cache($filename,0) or return 0;
        return $state->{_filecache}->{$filename}->{md5};
    }
}

# Returns true if the object's representation on disk (for file-like objects)
# or in ->{generated}->{_use_deps} (for non-file-like objects) is correct.
# Note that objects can be up to date even if they were changed on disk outside
# aimake; this will cause things that build-depend on them to become out of
# date, but not the file itself. Objects can also be up to date if an attempt
# to build from them failed, but not if an attempt to build them failed.
#
# The definition we use is "the last time the rule that was last used to
# produce this object was run, it produced this object, and nothing has
# changed that might cause the rule to be rerun."
my %utdcache = ();
sub up_to_date {
    my $objname = shift;
    exists $utdcache{$objname} and return $utdcache{$objname};
    # Things that are always up to date if they exist.
    $objname =~ /^(?:config_option|config_rule|cmd|intcmd|sys|optstring|optpath):/
        and return ($utdcache{$objname} = object($objname));
    $objname =~ /^([^:]+):/;
    my $objtype = $1;
    # Otherwise, check to see if all the dependencies in ->{generated}, and
    # their recursive dependencies, are the same as before.
    exists $state->{$objname} && exists $state->{$objname}->{generated}
        or return ($utdcache{$objname} = 0);
    my $g = $state->{$objname}->{generated};
    return ($utdcache{$objname} = 
            (rule_runnable($g->{_rule}, $g->{_object}) == 2) &&
            defined $state->{$g->{_object}}->{last_attempt}->
                {$g->{_rule}}->{_outputs}->{$objname} &&
            (!$filelike{$objtype} || object($objname, 'hash') eq
            $state->{$g->{_object}}->{last_attempt}->
                {$g->{_rule}}->{_outputs}->{$objname}));
}
# Complete list of things that can change whether an object is up to date:
# - The object's existence changes
# - The object's ->{generated} changes
# - The object's hash changes (i.e. the object was changed on disk)
# - The last_attempt of the rule/object pair that generated the object changes
# - For the rule that generated the object:
#   - The up-to-date status of an object in the command changed
#   - The up-to-date status of a use dep of an object in the command changed
#   - One of the rule's bpath outputs changed (perhaps a different one)
#   - The hash of an object or its use dep in the command changed
#   - The set of use dependencies of the command changed
# Complete list of things that can change the set of use dependencies of the
# command:
# - The _available containing a use dependency changed
# - The hash of an object in the generation history of a use dep changed
#   (objects are in their own generation histories of use deps)


# Given a rule and an object, returns 0 if the rule cannot be run on that
# object (due to missing dependencies or the like), 1 if the rule can be run
# on that object and it would do something, 2 if the rule can be run on that
# object but would do nothing (because nothing has changed since the last
# time it was run). In list context, also returns a reference to the use
# dependencies needed for the rule (to save having to recalculate them), if
# the output is 1.
sub rule_runnable {
    my $rulename = shift;
    my $object = shift;
    my $rule = $config->{rules}->{$rulename};
    if (ref $rule eq 'ARRAY') {
      IRULE_RULERUNNABLE: for my $irule (@$rule) {
            my @needed = (@{defined_or($irule->{'command'},[])},
                          @{defined_or($irule->{'outdepends'},[])});
            object($_) or (progress_report 4, "$_ blocks $rulename."),
                next IRULE_RULERUNNABLE for @needed;
            $rule = $irule;
            goto FOUND_IRULE_RULERUNNABLE;
        }
        progress_report 4, "No viable option found for $rulename.";
        return 0; # no rules applied
    }
  FOUND_IRULE_RULERUNNABLE:
    my $command = defined_or($rule->{'command'}, 'intcmd:nop');
    my @command = (ref $command ? (@$command) : ($command));
    push @command, "config_rule:$rulename";
    @command = grep { $_ ne '' } @command;
    map {append_if_incomplete(\$_, $object)} @command;
    @command = replace_extend(@command);
    progress_report 4,
        "Checking up-to-date: $rulename with $object (@command)";
    # If all the rule's immediate dependencies are up to date, then we
    # should consider the rule one that the user is attempting to run.
    up_to_date($_) or ((progress_report 4, "$_ is not up to date."),
                       return 0) for @command;
    $object eq 'sys:no_object' or up_to_date($object) or
        (progress_report 4, "Object $object is not up to date.");
    push @command, $object
        unless $object eq 'sys:no_object' ||
        $rule->{propagate_usedeps} || !$rule->{output};
    my @installdeps = ();
    $rule->{install_dir} and
        @installdeps = ('config_option:' . $rule->{install_dir});
    my @usedeps = expand_use_dependencies(
        ($object eq 'sys:no_object' ? [@command, @installdeps] :
         [$object, @installdeps]),
        \@command);
    push @usedeps, $object if $rule->{propagate_usedeps} || !$rule->{output}
        and $object ne 'sys:no_object';
    progress_report 4, "Checking expanded dependencies: " .
        "$rulename with $object (@usedeps)";
    my @broken = grep {!up_to_date($_)} @usedeps;
    @broken and $state->{_brokendeps}->{"$rulename $object"} = \@broken
        and (progress_report 4,
             "$rulename with $object is broken (@broken)"), return 0;
    my $last_try = $state->{$object}->{last_attempt}->{$rulename};
    progress_report 4, "Checking for dependency changes: " .
        "$rulename with $object";
    my %hfc = hashes_from_command(
        [@usedeps,"config_rule:$rulename"],
        defined $last_try && defined $last_try->{_bpathoutputs} ?
        $last_try->{_bpathoutputs} : []);
    # Check that none of the outputs were tampered with while we were
    # running, or while we weren't running. A change while we were
    # running forces us to error out (it's most likely due to a
    # mistake in a rule, e.g. telling a rule to place its output in
    # the wrong place). A change while we weren't running should
    # make us regenerate the file, if it's in the bpath.
    if (defined $hfc{_outputs} && keys %{$hfc{_outputs}}) {
        defined $bpathhash{$_}
        and $hfc{_outputs}->{$_} ne $bpathhash{$_}
        and die "Something changed $_ behind our back"
            for keys %{$hfc{_outputs}};
        $last_try->{_outputs}->{$_} ne $hfc{_outputs}->{$_} and
            (progress_report 2, "Something changed $_, regenerating it..."),
            (wantarray ? return (1, \@usedeps) : return 1)
            for keys %{$hfc{_outputs}};
    }
    # We want "set of dependencies are the same", = "keys of %hfc
    # equal keys of %$last_try", and "dependencies have the same
    # hashes", = "values of %hfc equal values of %$last_try".
    # So we're basically just doing a deep equality check.
    if (!defined $last_try->{_serial}) {
        $last_try->{_serial} = serialize_sv($last_try ,1);
    }
    (progress_report 4, "Nothing changed."), return 2
        if $last_try->{_serial} eq serialize_sv(\%hfc, 1);
    return 1 unless wantarray;
    return (1, \@usedeps);
}

# Given a list of dependencies as an array ref, expands it recursively until
# it's closed under use dependencies. Other arguments are used to avoid an
# infinite regress. The @orig array holds the original elements, in order to
# handle required_by correctly.
sub expand_use_dependencies {
    my @orig = replace_extend(@{+shift});
    my @args = replace_extend(@{+shift});
    my %seen = ();
    my @seen = ();
    # no progress_report because this is called recursively
    while (@args) {
        my $objname = shift @args;
        # TODO: expand_use_dependencies, object_deleted and object_created can
        # handle potential colons in the filename. Do they cause problems
        # elsewhere? (We're OK on systems where colons are directory
        # separators, because they'll be replaced with slashes in os2aimake.
        # The problem is random colons in filenames on, say, UNIX.)
        if ($objname =~ /^(symbol|file)_required_by:((?:[^:\\]|\\.)+):(.*)$/) {
            # We have to find out which symbol or file this is referring to.
            # We search _available for suitable files, and pick the most
            # appropriate. When choosing between files, we both have to choose
            # between local and global when appropriate (local is better
            # except for #include with angle brackets), between global files
            # (whichever one the compiler or linker would pick), and between
            # local files (this requires heuristics to determine which one
            # would be the most sensible to pick).
            #
            # In the case of files, the dependency generation will gives us an
            # spath: not a file: anyway if the file is on the compiler search
            # path, so we don't need to worry what the compiler would pick; it
            # doesn't know about any of the files in question anyway. As such,
            # we use the local files if one is available, and otherwise, we
            # assume that the user was one level out in their directory
            # structure and try removing and adding directory names.
            #
            # In the case of symbols, we make sure we only find the "best"
            # version of each library via the rules, so we only have one
            # global library to worry about. We pick local libraries and/or
            # object files if they have all the symbols required, and
            # otherwise use a global. The method used to locate libraries is
            # designed to ensure that only the library that the linker would
            # consider the best version of the library is used.

            my $index = $1;
            my $name = $2;
            my $direct_requirer = $3;
            $name =~ /((?:[^\\\/]|\\.)+)$/;
            my $basename = $1;
            my $available = $state->{_available}->{"$index:$basename"};
            $name = '' if $index eq 'symbol';

            if ($available) {
                my @available = keys %$available;
                @available = grep { object_preference(
                                        $_,[@orig,$direct_requirer],
                                        $name) > 0 }
                    @available;
                if (scalar @available) {
                    # The $a cmp $b here is a tiebreak to ensure the same
                    # object is always picked in case of a tie.
                    ($objname, undef) =
                        sort {object_preference($b,\@orig,$name) <=>
                              object_preference($a,\@orig,$name) ||
                              object_preference($b,[$direct_requirer],$name) <=>
                              object_preference($a,[$direct_requirer],$name) ||
                              $a cmp $b} @available;
                }
            } # otherwise there's no such object; fall through
        }
        next if $seen{$objname};
        $seen{$objname} = 1;
        push @seen, $objname;
        push @args, use_dependencies($objname, @_);
    }
    return @seen;
}
# Determining use dependencies is easy, because dependency rules are run the
# same way as provicion rules. We know which rules should be generating
# dependencies, so we list them.
sub use_dependencies {
    my $objname = shift;
    my @deps;
    $config->{rules}->{$_}->{depends} and
        push @deps, "dependencies:$_:$objname" for @{$applicable{$objname}};
    if (exists $state->{$objname} && exists $state->{$objname}->{generated}) {
        push @deps, (@{$state->{$objname}->{generated}->{_use_deps}})
            if exists $state->{$objname}->{generated}->{_use_deps};
    }
    return @deps;
}

# Given an object that might potentially fulfil a dependency, and list of
# objects that together require that dependency, return a number indicating
# how well they match.
sub object_preference {
    my $depended = shift;
    my @requirers = @{+shift};
    my $wantedname = shift;

    @requirers = grep /^b?path:/, @requirers;

    scalar @requirers == 0 and return 1;
    if (scalar @requirers > 1) {
        my $bestpref = 0;
        for my $requirer (@requirers) {
            my $score = object_preference($depended,[$_],$wantedname);
            $bestpref < $score and $bestpref = $score;
        }
        return $bestpref;
    }
    my $requirer = $requirers[0];
    # We prefer bpath: and path: equally. spath: is less preferred than
    # anything in the path. Within the spath:, we try to pick something that
    # ends with $name if possible, otherwise make an arbitrary choice (that
    # ends with $basename). Within the path: and bpath:, we require the
    # containing object to end with $basename (fuzziness in the paths of
    # system objects is reasonable, in the paths of source objects is
    # ridiculous), and pick the one with the most consecutive matching letters
    # at the start of the name (including slashes).
    $requirer =~ /path:(.*)$/; # could be bpath: or spath: too
    my $requirepath = $1;
    $depended =~ /path:(.*)$/;
    my $dependpath = $1;
    my $score;
    if ($depended =~ /\bspath:/) {
        # Low scores. Slightly higher if $wantedname is matched.
        $score = $dependpath =~ /\Q$wantedname\E$/ ? 2 : 1;
    } else {
        # Higher scores, depending on how many characters match.
        # Disallow if $wantedname doesn't match, though.
        $score = 0;
        if ($dependpath =~ /\Q$wantedname\E$/) {
            $score = 2;
            my ($c1, $c2);
            do {
                $score++;
                $requirepath =~ s/^(.)//;
                $c1 = $1;
            $dependpath =~ s/^(.)//;
                $c2 = $1;
            } while (defined $c1 and defined $c2 and $c1 eq $c2);
        }
    }
    progress_report 4, "Checking $depended with $requirer for $wantedname: $score";
    return $score;
}
# This is based entirely on names, so we can memoize it.
# Because array references can be given as arguments, it needs normalization.
BEGIN { memoize('object_preference',
                NORMALIZER => sub {return serialize_sv([@_])}); }

# Given parts of a rule, runs a command and processes the output.
# This assumes that all relevant dependencies have already been checked.
sub run_rule_command {
    my @command = @{+shift};       # already expanded with all use deps
    my $filter = shift;            # nullable
    my $out2stem = shift;          # single string or list, can contain regexen
    my $out2arg = shift;           # single string, regex, or undef
    my $out23inner = shift;        # single string or regex
    my $out3stem = shift;          # like out2stem, can also be undef
    my $out3arg = shift;           # like out2arg
    my $filter_spath = shift;      # true or false
    my $filter_text_files = shift; # true or false
    my $linesep = shift;           # line separator
    my $unescape = shift;          # unescaping algorithm
    my $lineskip = shift;          # lines to skip at the start
    my $linemax = shift;           # maximum number of lines to read
    my $in_subdir = shift;         # directory name or undef

    # Progress message is printed by caller
    my ($failreason, $stdout, $stderr) =
        defined $in_subdir ? run_command(@command, \("subdir:" . $in_subdir)) :
        run_command(@command);
    # We parse the output even on failure. The caller might care.

    # Looks like it succeeded. Now we just have to parse the output.
    progress_report 4, "Parsing output (inner = $out23inner)";
    my $rx = undef;
    my $objnamy = scalar grep /^intcmd:list/, @command;
    (ref $out2stem) =~ /\bRegexp$/ and $rx = $out2stem;
    (ref $out2arg) =~ /\bRegexp$/ and ($rx, $objnamy) = ($out2arg, 1);
    (ref $out23inner) =~ /\bRegexp$/ and $rx = $out23inner;
    (ref $out3stem) =~ /\bRegexp$/ and $rx = $out3stem;
    (ref $out3arg) =~ /\bRegexp$/ and ($rx, $objnamy) = ($out3arg, 1);
    my $rxreplace = [];
    if ($rx) {
        if (!ref $stdout) {
            # $stdout needs a split and perhaps an unescape.
            defined $linesep or $linesep = "\n";
            (ref $linesep) =~ /\bRegexp$/ and $linesep = quotemeta $linesep;
            if (defined $unescape) {
                $linesep = qr/(?<!\\)(?:\\\\)*\K$linesep/;
            }
            $stdout = [split /$linesep/, $stdout];
            if (defined $unescape && $unescape eq 'backslash') {
                s/\\\n//g for @$stdout;
                s{\\(.)}{do{my $x = $1; $x =~ y/abfnrt/\a\b\f\n\r\t/; $x}}ge
                    for @$stdout;
            } elsif (defined $unescape && $unescape eq 'backslash_whitespace') {
                s/\\\n//g for @$stdout;
                s/\\([\s\\])/$1/ge for @$stdout;
            }
        }
        my @outlines = $filter ? grep /$filter/, @$stdout : @$stdout;
        for my $line (@outlines) {
            $lineskip--, next if $lineskip;
            next if defined $linemax && $linemax == 0;
            $linemax-- if defined $linemax;
            while ($line =~ m/$rx/g) {
                my $n = $1;
                $objnamy or $n = os2aimake($n, $out23inner);
                push @$rxreplace, $n unless
                    ($n =~ /^spath:/ && $filter_spath) ||
                    ($filter_text_files &&
                     $n =~ /^[bs]?path:/ && -T object($n, 'stringify'));
            }
        }
    }
    (ref $out2stem) =~ /\bRegexp$/ and $out2stem = $rxreplace;
    (ref $out2arg) =~ /\bRegexp$/ and $out2arg = $rxreplace;
    (ref $out23inner) =~ /\bRegexp$/ and $out23inner = $rxreplace;
    (ref $out3stem) =~ /\bRegexp$/ and $out3stem = $rxreplace;
    (ref $out3arg) =~ /\bRegexp$/ and $out3arg = $rxreplace;
    defined $out3stem or $out3stem = ''; # so we can regexp it, it'll be ignored
    ref $out2stem eq 'ARRAY' or $out2stem = [$out2stem];
    defined $out2arg and ref $out2arg eq 'ARRAY' || ($out2arg = [$out2arg]);
    defined $out3arg and ref $out3arg eq 'ARRAY' || ($out3arg = [$out3arg]);
    ref $out23inner eq 'ARRAY' or $out23inner = [$out23inner];
    ref $out3stem eq 'ARRAY' or $out3stem = [$out3stem];
    my @out2 = ();
    for my $stem (@$out2stem) {
        for my $inner (@$out23inner) {
            if (defined $out2arg) {
                for my $arg (@$out2arg) {
                    my $x = $stem.$arg;
                    append_if_incomplete(\$x, $inner);
                    push @out2, $x;
                }
            } else {
                my $x = $stem;
                append_if_incomplete(\$x, $inner);
                push @out2, $x;
            }
        }
    }
    my @out3 = ();
    for my $stem (@$out3stem) {
        for my $inner (@$out23inner) {
            if (defined $out3arg) {
                for my $arg (@$out3arg) {
                    my $x = $stem.$arg;
                    append_if_incomplete(\$x, $inner);
                    push @out3, $x;
                }
            } else {
                my $x = $stem;
                append_if_incomplete(\$x, $inner);
                push @out3, $x;
            }
        }
    }
    @out2 = replace_extend(@out2);
    @out3 = replace_extend(@out3);

    return ($failreason, \@out2, \@out3,
            !defined $stderr || $stderr eq '' ? undef : $stderr);
}

# Given a command, returns a list of pairs from objects in the command and
# their dependencies, to their hashes. This expands build dependencies that
# were used for generation, not use dependencies (that's assumed to have been
# done already by the caller). The purpose of this is to remember what
# dependencies a command had, to determine if it needs to be rerun. The second
# argument is a list of outputs from that command; the purpose of that is to
# know if any of the command's outputs have been tampered with (so that they
# can be reconstructed in that case). We only consider tampering on the bpath
# (otherwise, we weren't actually producing the output, we were merely noting
# its existence).
sub hashes_from_command {
    my %out;
    my @command = @{+shift};
    my @outputs = @{+shift};
    progress_report 4, "Calculating remembered dependencies...";
    while (@command) {
        my $objname = shift @command;
        $objname =~ /^([^:]+):/;
        my $objtype = $1;
        # File-like objects, we recurse through the dependencies.
        $out{$objname} = object($objname, 'hash') if $filelike{$objtype};
        # Non-file-like objects, we copy the dependencies.
        if (exists $state->{$objname} &&
            exists $state->{$objname}->{generated} && !$filelike{$objtype}) {
            my $g = $state->{$objname}->{generated};
            my $h = $state->{$g->{_object}}->{last_attempt}->{$g->{_rule}};
            /^_/ or $out{$_} = $h->{$_} for keys %$h;
        }
        # And we record the fact we used them (in case dependency changes
        # cause a change in which command-line options are used, for
        # instance).
        if (!$filelike{$objtype}) {
            $out{$objname} = 'used';
        }
    }
    while (@outputs) {
        my $objname = shift @outputs;
        $objname =~ /^([^:]+):/;
        my $objtype = $1;
        $out{_outputs}->{$objname} = 'generated';
        $out{_outputs}->{$objname} = object($objname, 'hash')
            if $filelike{$objtype};
    }
    return %out;
}

# Called whenever an object is changed; performs cache invalidation.
# The caches are caches that only last for a single run anyway, so there's no
# need to worry about calling this between runs. (object_created /does/ last
# longer than a single run; but objects cannot be created outside aimake.)
sub object_changed {
    my $objname = shift;
    progress_report 4, "Invalidating caches of $objname...";
    $objname =~ /^(?:path|bpath|spath):/ and
        delete $filecache{object($objname, 'stringify')};
    %utdcache = ();
    # %applicable contains matches related to the object's /name/, so a mere
    # change to the object isn't enough to invalidate it. However, if this is a
    # new object, it might not exist in the first place.
    exists $applicable{$objname} or update_applicable($objname);
}

# Called whenever an object is created. Calls object_changed.
sub object_created {
    my $objname = shift;
    $objname =~ /^symbol_in_object:([^:]+)/ and
        $state->{_available}->{"symbol:$1"}->{$objname} = 0;
    $objname =~ /^[bs]?path:.*?((?:[^\\\/]|\\.)+)$/
        and $state->{_available}->{"file:$1"}->{$objname} = 0;
    object_changed($objname);
}

# Called to delete an object. Objects should not be deleted except via this
# method. In the case of bpath: objects, also deletes them from the
# filesystem. In the case of non-file-like objects, deleting them from the
# statefile effectively destroys them, because they only exist in the
# statefile. In the case of other file-like objects, the caller is responsible
# for ensuring that the backing file was either deleted outside aimake, or is
# no longer relevant. TODO: Add some way of specifying and deleting temporary
# files like aimake_1.exe.
sub delete_object {
    my $objname = shift;
    return if $objname eq 'sys:no_object'; # holds useful _last_attempt info
    progress_report 4, "Deleting ungeneratable object $objname...";
    if (up_to_date($objname)) {
        %utdcache = ();
    } # otherwise deleting the object can't make change up to date status
    if ($objname =~ /^bpath:/) {
        # Ignore errors in unlinking the object. It might not have existed.
        unlink object($objname, 'stringify');
    }
    $objname =~ /^(?:path|bpath|spath):/ and
        delete $filecache{object($objname, 'stringify')};
    $objname =~ /^symbol_in_object:([^:]+)/ and
        delete $state->{_available}->{"symbol:$1"}->{$objname};
    $objname =~ /^[bs]?path:.*?((?:[^\\\/]|\\.)+)$/
        and delete $state->{_available}->{"file:$1"}->{$objname};
    if (exists $applicable{$objname}) {
        delete $applicable{"_$_"}->{$objname} for @{$applicable{$objname}};
        delete $applicable{$objname};
    }
    delete $state->{$objname};
}

# Updates %applicable to be the list of rules that apply to the object given.
sub update_applicable {
    my $objname = shift;
    my @rules = ();
    for my $rulename (sort keys %{$config->{rules}}) {
        my $rule = $config->{rules}->{$rulename};
        ref $rule eq 'ARRAY' and next;
        exists $rule->{object} or next;
        ((ref $rule->{object}) =~ /\bRegexp$/ &&
         $objname =~ $rule->{object}) ||
            (!ref $rule->{object} && $objname eq $rule->{object}) and
            push @rules, $rulename;
    }
    $applicable{$objname} = \@rules;
    $applicable{"_$_"}->{$objname} = 1 for @rules;
}

# Hash calculation.
sub config_hash {
    my $configkey = shift;
    my $innerkey = shift;
    return sv_hash($config->{$configkey}->{$innerkey});
}
BEGIN { memoize('config_hash'); } # config stays the same in one program run

# Returns the hash of an arbitrary scalar. We'd like to be able to just run
# Storable::freeze and hash that, but it's buggy; it'll return a different
# value on a number that's been used as a string, and a number that hasn't
# been used as a string. So instead, we write our own serialization
# routine. (One nice thing about just determining hashes is that we don't
# need a matching deserialization routine.) We only handle the data types
# we actually use.
sub sv_hash {
    return md5_hex(serialize_sv(shift));
}
sub serialize_sv {
    my $sv = shift;
    my $ignore_keys_starting_with_underscore = shift;
    defined $sv or return 'u';
    # Make sure we're always using the string version of $sv.
    ref $sv or return "s" . (length "$sv") . " $sv";
    (ref $sv) =~ /\bRegexp$/ and return "r" . (length "$sv") . " $sv";
    if (ref $sv eq 'ARRAY') {
        my $x = "a" . scalar @$sv . " ";
        $x .= serialize_sv($_) for @$sv;
        return $x;
    }
    if (ref $sv eq 'HASH') {
        my @keys = sort keys %$sv;
        @keys = grep +(/^[^_]/), @keys
            if $ignore_keys_starting_with_underscore;
        my $x = "h" . scalar @keys. " ";
        $x .= serialize_sv($_). serialize_sv($sv->{$_}) for @keys;
        return $x;
    }
    die "serialize_sv called on unsupported reference type " . ref $sv;
}

sub update_file_cache {
    my $filename = shift;
    my $needcontents = shift;
    # If the file hasn't been removed from the cache, assume it hasn't changed
    # since we last looked at it earlier in the program run. (%filecache has
    # to be invalidated whenever a file is changed.)
    exists $filecache{$filename} and return 1;
    # Due to a quirk in Perl, the -l has to come first.
    -l $filename || -f _ or          # can't update irregular files
        (delete $state->{_filecache}->{$filename}), return;
    my $modtime = (stat(_))[9]; # 9 = modification time
    my $filechanged = $modtime !=
        defined_or($state->{_filecache}->{$filename}->{modtime},-1);
    if (!$filechanged && !$needcontents) {return 1;}
    progress_report 2.5, "Reading file '$filename'...";
    # The current contents of the file in disk aren't loaded into memory, so
    # do that now.
    if ($filechanged || $needcontents) {
        local $/;
        open my $fh, '<:raw', $filename or    # can't update unreadable files
            (delete $state->{_filecache}->{$filename}), return;
        $filecache{$filename} = <$fh>;
        close $fh;
    }
    # No need to recalculate the hash unless the file actually changed on disk.
    if ($filechanged) {
        $state->{_filecache}->{$filename}->{modtime} = $modtime;
        $state->{_filecache}->{$filename}->{md5} =
            md5_hex($filecache{$filename});
    }
    return 1;
}

# Interfacing with OS stuff.

# This is the same idea as IPC::Cmd->can_run, but written using only
# modules that existed with 5.8.
sub locate_command {
    my $cmd = shift;
    for my $dir (File::Spec->path) {
        my $fnos = File::Spec->rel2abs($cmd, $dir);
        my $cmdos = MM->maybe_command($fnos);
        return $cmdos if $cmdos;
    }
    return;
}
BEGIN { memoize('locate_command'); }

# Find the version of a command that's available. We do this using the
# --version option, which the vast majority of programs recognise nowadays
# (and the few that don't are likely to error out on). GNU standards specify
# that the first line of version output be in a particular format, but many
# programs don't respect that, so we simply take the first nonempty line of
# output as the version number.
sub command_version {
    my $cmdobject = shift;
    my ($failreason, $stdout, $stderr) =
        run_command($cmdobject, 'optstring:--version');
    return '?' if $failreason;
    my $output = defined_or($stdout,'') . defined_or($stderr,'');
    my @output = split /\r?\n/, $output;
    shift @output while @output && $output[0] eq '';
    return '?' unless @output; # program produced no version output
    return $output[0];
}
BEGIN { memoize('command_version'); }

# aimake has its own portable-between-OSes path format for use in
# config files, so that they can be written in a system-independent
# way. Here's how to actually use the paths on the filesystem.
sub aimake2os {
    my $aipath = shift;
    my $basedir = shift;
    $aipath eq "" and return $basedir;
    my @aipath = split m=(?<!\\)(?:\\\\)*\K/=, $aipath;
    s/\\(.)/$1/g for @aipath;
    my $ospath = File::Spec->catfile(@aipath);
    $ospath = File::Spec->rel2abs($ospath, $basedir);
    return $ospath;
}
BEGIN { memoize('aimake2os'); }
# The same, for spath:.
sub aimake2oss {
    my $spath = shift;
    my @aipath = split m=(?<!\\)(?:\\\\)*\K/=, $spath;
    s/\\(.)/$1/g for @aipath;
    my $volume = shift @aipath;
    my $file = pop @aipath;
    unshift @aipath, File::Spec->rootdir();
    return File::Spec->catpath($volume, File::Spec->catdir(@aipath), $file);
}
BEGIN { memoize('aimake2oss'); }
# And in reverse. Absolute paths become path:/bpath:/spath: as appropriate.
# Relative paths become file_required_by:, and the second argument specifies
# what's requiring them.
sub os2aimake {
    my $ospath = shift;
    my $requirer = shift;
    if (File::Spec->case_tolerant) {
        # Force the filename to a consistent case, on systems that use
        # case-insensitive filesystems.
        $ospath = defined_or(
            eval 'use feature ":5.16"; fc $ospath',
            lc $ospath);
    }
    if (File::Spec->file_name_is_absolute($ospath)) {
        $ospath = defined_or(absolute_path($ospath),
                             File::Spec->canonpath($ospath));
        my $osrel = ospath_inside($ospath, $state->{_srcdir});
        my $objtype;
        if ($osrel) {
            $objtype = 'path:';
        } else {
            $osrel = ospath_inside($ospath, $cwd);
            if (!$osrel) {
                # We have to form an absolute path for the spath.
                my ($vol, $dirs, $file) = File::Spec->splitpath($ospath);
                my @dirs = grep {$_ ne ''} File::Spec->splitdir($dirs);
                map s/([:\/\\])/\\$1/g, @dirs;
                $file =~ s/([:\/\\])/\\$1/g;
                $vol  =~ s/([:\/\\])/\\$1/g;
                return "spath:" . join('/', $vol, @dirs, $file);
            } else {
                $objtype = 'bpath:';
            }
        }
        $osrel eq File::Spec->curdir and return $objtype;
        my (undef, $dirs, $file) = File::Spec->splitpath($osrel);
        my @dirs = grep {$_ ne ''} File::Spec->splitdir($dirs);
        map s/([:\/\\])/\\$1/g, @dirs;
        $file =~ s/([:\/\\])/\\$1/g;
        return $objtype . join('/', @dirs, $file);
    } else {
        (ref $requirer) =~ /\bRegexp$/ || $requirer eq 'sys:no_object'
            and die "Absolute pathname $ospath found with empty or regex inner";
        my (undef, $dirs, $file) = File::Spec->splitpath($ospath);
        my @dirs = File::Spec->splitdir($dirs);
        map s/([:\/\\])/\\$1/g, @dirs;
        $file =~ s/([:\/\\])/\\$1/g;
        return "file_required_by:" . join('/', @dirs, $file) . ":$requirer";
    }
}
# Helper function: returns true (specifically, the relative path) if one path
# is inside another. In list context, also returns the number of directories
# between them (or -1 if not nested).
sub ospath_inside {
    scalar @_ == 2 and defined $_[0]
        or die "ospath_inside requires 2 arguments";
    my $inside = absolute_path(shift);
    my $outside = absolute_path(shift);
    return unless defined $inside && defined $outside;
    my $difference = File::Spec->abs2rel($inside, $outside);
    return unless defined $difference;
    return if $difference eq $inside; # different volumes
    my (undef, $dirs, undef) = File::Spec->splitpath($difference);
    my @dirs = File::Spec->splitdir($dirs);
    @dirs = grep { $_ ne '' } @dirs;
    $difference eq File::Spec->curdir and
        wantarray ? return ($difference, 0) : return $difference;
    # $inside is inside $outside if $inside's path relative to
    # $outside doesn't need to use .. or the like.
    scalar @dirs == scalar File::Spec->no_upwards(@dirs)
        and wantarray ? return ($difference, 1 + scalar @dirs) :
        return $difference;
    return unless wantarray;
    return (undef, -1);
}

sub absolute_path {
    my $ospath = shift;
    -e $ospath or return undef;
    my $apath = abs_path($ospath);

    if (File::Spec->case_tolerant) {
        # Force the filename to a consistent case, on systems that use
        # case-insensitive filesystems.
        $apath = defined_or(
            eval 'use feature ":5.16"; fc $apath',
            lc $apath);
    }
    return $apath;
}

# Given a list of objects, replaces any extend: pseudo-objects with
# actual objects.
# TODO: A better algorithm with fewer clashes and nicer output.
sub replace_extend {
    map {
        # extend:..: removes filenames, leaving a directory.
        s/\bextend:\.\.:([bs]?path:.*?)\/(?:[^\\\/]|\\.)+$/$1/s;
        # Other extend: changes the extension and forces the result to be
        # placed in the bpath.
        s/\bextend:([^:]*):[bs]?path:(.*?)(?:\.(?:[^\\\/]|\\.)+)?$/
            "bpath:$2$1"/se
            and ensure_directory(aimake2os("$2$1", $cwd));
    } @_;
    return @_;
}

# Given an object name, produce a nicer name for display to the user.
sub shorten_object {
    my $objname = shift;
    $objname =~ s/^([^:]+)(?:_in_object|_required_by):([^:]+):.*$/$1:$2/;
    $objname =~ s/^searchpath:[^:]+://;
    $objname =~ /^spath:/ and return aimake2oss($objname);
    $objname =~ s/^b?path://;
    return $objname;
}

# Given an object name, append another object to it if it the outside
# part of a nested object.
sub append_if_incomplete {
    my $onameref = shift;
    my $inner = shift;
    $$onameref eq '' and ($$onameref = $inner), return;
    $$onameref =~ /^([^:]+):([^:]*+):?(.*)$/ or die
        "Invalid object name '$$onameref'";
    my ($objtype, $objvalue, $objinner) = ($1, $2, $3);
    $nested{$objtype} or return;
    if ($objinner eq '') {
        $$onameref = "$objtype:$objvalue:$inner";
    } else {
        append_if_incomplete(\$objinner, $inner);
        $$onameref = "$objtype:$objvalue:$objinner";
    }
}

# Ensure the directory that contains the given OS path exists.
sub ensure_directory {
    my $ospath = shift;
    my ($vol, $dirs, $file) = File::Spec->splitpath($ospath);
    my $dir = File::Spec->catpath($vol, $dirs, '');
    return if -d $dir; # avoid a useless message
    die "Found a non-directory at $dir" if -e $dir;
    progress_report 2, "Creating directory $dir...";
    make_path $dir;
}

# Running commands. We're given a list of build dependencies, and somehow have
# to assemble a command out of them. We do this via using the
# OS-stringification of the command, then all the options. Command-like
# objects (cmd:, intcmd:) come first, and we demand exactly one of them. (If
# neither cmd: nor intcmd: is given, we use an executable bpath: dependency,
# if there is one.) Then option-like objects (optstring:, optpath:). Other
# objects are omitted (because they exist to specify option-like or
# command-like objects as use dependencies).
sub run_command {
    my @execs = grep /^(?:cmd|intcmd):/, @_;
    my @opts = grep /^opt(?:string|path):/, @_;
    # For commands like flex, we have to put file-like objects last.
    # So optstring comes before optpath, and optpath with an argument
    # comes before optpath without an argument.
    # Some options (e.g. libraries) have to come last.
    # However, we also need the arguments to stay in the same order,
    # otherwise. So we just grep the various categories.
    @opts = ((grep /^optstring:[^ ]/, @opts),
             (grep !/^optstring:./ && !/^optpath::/, @opts),
             (grep /^optpath::/, @opts),
             (grep /^optstring: /, @opts));
    my @intopts = grep ref $_, @_;
    local $" = ", ";
    if (scalar @execs == 0) {
        @execs = grep /^bpath:/, @_;
        @execs = grep {-x object($_,"stringify")} @execs;
    }
    scalar @execs == 1
        or die "Invalid set of command use deps: {@_}";
    my $op = "stringify";
    if (@intopts && ${$intopts[0]} =~ /^subdir:(.*)$/s) {
        chdir object($1, "stringify") or
            return ("Could not change directory to '".
                    object($1, "stringify")."'", '', '');
        $op = "stringify-relative";
    }
    my @cmdline = (map {object($_,$op)} @opts);
    if (@intopts && ${$intopts[0]} eq 'dontrun') {
        # We use Perl escaping to show where the arguments start and end.
        # This probably won't be the same as the user's native escaping method
        # (which aimake never deals with), but it should be intelligible.
        unshift @cmdline, $execs[0] =~ /^intcmd:/ ? $execs[0] :
            object($execs[0], "stringify");
        @cmdline = map { my $x = Dumper($_); chomp $x; $x } @cmdline;
        return join ' ', @cmdline;
    }
    my @rv = (object($execs[0], "exec")->(@cmdline, @intopts));
    chdir $cwd;
    return @rv;
}

# Progress reporting.
BEGIN {{
    my $colwidth = undef;
    sub screenwidth {
        my $failreason;
        $colwidth and return $colwidth;
        object("cmd:tput") and ($failreason, $colwidth) =
            run_command("cmd:tput","optstring:cols", \ "nostderr")
            and !$failreason and return $colwidth;
        $colwidth = 80; return $colwidth;
    }
    }}
BEGIN {{
    my $last_report_msg = '';
    sub progress_report {
        my $level = shift;
        my $str = shift;
        if ($level <= $options{v} && !$options{r}) {
            # We may have to clear the line the old-fashioned way.
            print STDERR ' ' x (length defined_or($last_report_msg,"")), "\r";
            $last_report_msg = '';
        }
        my $ldiff = (length $str) - length defined_or($last_report_msg,"");
        $ldiff < 0 and $str .= ' ' x -$ldiff;
        if ($level <= $options{v} + 1.5 && $level > $options{v}
            && $level < 3 && !$options{r}) {
            $str = substr $str, 0, screenwidth() - 1;
            print STDERR "$str\r";
            $str =~ s/ +$//;
            $last_report_msg = $str;
        } elsif ($level <= $options{v}) {
            print STDERR "$str\n";
            $last_report_msg = '';
        }
    }
    }}

__DATA__
# aimake config file. This is written in a very limited dialect of
# Perl (which only allows scalar, regex, array, and hash constants,
# and assigning to and using scalar variables), and determines the
# behaviour on different sorts of files.
#
# This file is the global file that specifies default behaviour for
# aimake, and is designed to work on a wide range of projects (and
# hopefully, eventually, operating systems). Projects can use their
# own specific aimake2.config file to override the config in this
# file.
#
# See `perldoc -F aimake` for the format.
{
    options => {
        default_libraries => [
            'c',
            'msvcrt', # Windows version of libc
            'm',
        ],
        libraries => [],
        ignore_directories =>
            qr/^(?:\.svn|\.git|\.hg|\.bzr|_darcs)$/i,
        ignore_directories_with_files => [
            'aimake.state',   # old versions of aimake
            'aimake2.state',  # this version of aimake
            'config.status',  # autoconf
            'CMakeCache.txt', # CMake
        ],
        # Directories.
        # Relative to -i on the command line.
        bindir => 'bin',
        libdir => 'lib',
        specificlibdir => 'lib/$packagename',
        includedir => 'include',
        specificincdir => 'include/$packagename',
        datarootdir => 'share',
        configdir => 'etc',    # can plausibly be /etc
        staterootdir => 'var', # can plausibly be /var
        # Relative to datarootdir.
        datadir => '$datarootdir/$packagename',
        mandir => '$datarootdir/man',
        infodir => '$datarootdir/info',
        docdir => '$datarootdir/doc/$packagename',
        # Relative to staterootdir.
        statedir => '$staterootdir/lib/$packagename',
        logdir => '$staterootdir/log',
        lockdir => '$staterootdir/run',
        specificlockdir => '$staterootdir/run/$packagename',
    },
    rules => {
        # Option sets.
        # TODO: Take these from options? Or from the environment?
        default_cflags => {
            output => 'optionset:CFLAGS',
            outdepends => 'optstring:-g -O2 -Wall',
            verb => 'determined',
        },
        default_iflags => {
            output => 'optionset:IFLAGS',
            outdepends => [],
            verb => 'determined',
        },
        default_lflags => {
            output => 'optionset:LFLAGS',
            outdepends => [],
            verb => 'determined',
        },
        cflags_includes_iflags => {
            object => 'optionset:CFLAGS',
            depends => 'optionset:IFLAGS',
            verb => 'determined',
        },

        default_libraries => {
            output => 'optionset:libraries',
            # some likely guesses, not including the system libc
            outdepends => ['optstring:-lcrypt',
                           'optstring:-ldl',
                           'optstring:-lm',
                           'optstring:-lncurses',
                           'optstring:-lncursesw',
                           'optstring:-lpthread',
                           'optstring:-lrt',
                           'optstring:-lz'],
            verb => 'determined',
        },

        # Finding relevant files.
        find_source_files => {
            # "optpath::path:" = root of the source directory
            command => ['sys:always_rebuild', 'intcmd:listtree',
                        'optpath::path:'],
            output => qr/(.+)/s,
            verb => 'found',
        },

        # Finding header files is difficult. The problem is that cpp knows the
        # search path, but not even GNU cpp will tell you the search path upon
        # asking. As such, our solution is to preprocess a small test file
        # that includes some headers, and see where those headers are. In
        # order to accomplish this, we need to choose one header on each of
        # the likely search paths. We use the following set of headers:
        #
        # <iso646.h>
        #   Some C compilers provide their own set of headers independent of
        #   the OS's header files. <iso646.h> is a good choice because it's
        #   trivial to write, standard C, completely system-independent, and
        #   not provided by some system libraries (and as such, a compiler
        #   will want to patch around the deficiency). For instance, gcc and
        #   clang both have a copy, glibc doesn't.
        # 
        # <limits.h>
        #   Some C compilers try to fix brokenness in the system include
        #   files; this involves making their own private copies elsewhere.
        #   <limits.h> is the only file that's unconditionally fixed by gcc on
        #   every system, so we have to use it. (As a bonus, it also seems to
        #   be unconditionally fixed by clang; its fix is on the same search
        #   path as its iso646.h, but including it makes it include /gcc's/
        #   fixed <limits.h>, if both are installed, thus informing us of a
        #   path the system knows about. And it's also standard C.)
        #
        # <sys/types.h>
        #   Some compilers have a multiple-architecture system, where
        #   different header files are used depending on the target
        #   architecture. As such, we want to find the correct directory for
        #   the architecture. None of the standard C header files are involved
        #   with this system (which means that it doesn't matter for standard
        #   C), so we use a standard POSIX header, <sys/types.h>, which is
        #   obviously highly architecture-dependent. (glibc provides a
        #   multiarch version of sys/types.h, as well as a default version.)
        #
        # <setjmp.h>
        #   We need to find the default system libraries themselves. This
        #   involves picking one that won't be involved in multiarch, will be
        #   provided by the libc not the compiler, and won't be fixed by the
        #   compiler's installation. The only standard C header that fulfils
        #   these conditions is <setjmp.h>, probably because it's too weird
        #   to be caught up in any of the other mechanisms.
        #
        # <zlib.h>
        #   Finally, some C installations use a separate path for libraries
        #   that are not part of C or POSIX. zlib is chosen because it has an
        #   excellent chance of being installed by such installations (it's
        #   not only commonly used by itself, it's also a dependency of lots
        #   of other things).
        #
        # We use the resulting search paths for two things:
        # 
        # a) To tell aimake about the existence of header files, so that when
        #    a file tries to include <stdio.h> or the like and the compiler
        #    tells aimake where that is, aimake won't refuse to run on the
        #    basis that it doesn't know about that file.
        #
        # b) To find header files that have been specified in slightly wrong
        #    directories. This makes programs much more portable, because the
        #    directory they expect to find a header in isn't always the
        #    directory it's actually in.
        generate_search_test_file => {
            command => [
                'intcmd:writefile', 'optpath::bpath:aimake/aimake_1.c',
                'optstring:' .
                '#ifdef_AIMAKE11 #include_<iso646.h> #endif ' .
                '#ifdef_AIMAKE12 #include_<limits.h> #endif ' .
                '#ifdef_AIMAKE13 #include_<sys/types.h> #endif ' .
                '#ifdef_AIMAKE14 #include_<setjmp.h> #endif ' .
                '#ifdef_AIMAKE15 #include_<zlib.h> #endif ' .
                'int_main(void)_{return_0;}'],
            output => 'bpath:aimake/aimake_1.c',
            verb => 'generated',
        },
        # We rely on the compiler specifying the header file we included as
        # the first dependency that contains its name. This seems pretty
        # likely, really.
        locate_compiler_provided_include_path => {
            command => ['tool:c_dependencies', 'bpath:aimake/aimake_1.c',
                        'optpath::bpath:aimake/aimake_1.c',
                        'optstring:-DAIMAKE11'],
            linesep => ' ', linemax => 1, unescape => 'backslash_whitespace',
            filter => qr/\biso646\.h\b/, inner => qr/^(.+)\biso646\.h/,
            output => 'searchpath:systeminclude:',
            verb => 'located',
        },
        locate_compiler_patched_include_path => {
            command => ['tool:c_dependencies', 'bpath:aimake/aimake_1.c',
                        'optpath::bpath:aimake/aimake_1.c',
                        'optstring:-DAIMAKE12'],
            linesep => ' ', linemax => 1, unescape => 'backslash_whitespace',
            filter => qr/\blimits\.h\b/, inner => qr/^(.+)\blimits\.h/,
            output => 'searchpath:systeminclude:',
            verb => 'located',
        },
        locate_multiarch_include_path => {
            command => ['tool:c_dependencies', 'bpath:aimake/aimake_1.c',
                        'optpath::bpath:aimake/aimake_1.c',
                        'optstring:-DAIMAKE13'],
            linesep => ' ', linemax => 1, unescape => 'backslash_whitespace',
            filter => qr/\bsys.*?types\.h\b/, inner => qr/^(.+)\bsys.*?types\.h/,
            output => 'searchpath:systeminclude:',
            verb => 'located',
        },
        locate_libc_include_path => {
            command => ['tool:c_dependencies', 'bpath:aimake/aimake_1.c',
                        'optpath::bpath:aimake/aimake_1.c',
                        'optstring:-DAIMAKE14'],
            linesep => ' ', linemax => 1, unescape => 'backslash_whitespace',
            filter => qr/\bsetjmp\.h\b/, inner => qr/^(.+)\bsetjmp\.h/,
            output => 'searchpath:systeminclude:',
            verb => 'located',
        },
        locate_non_libc_include_path => {
            command => ['tool:c_dependencies', 'bpath:aimake/aimake_1.c',
                        'optpath::bpath:aimake/aimake_1.c',
                        'optstring:-DAIMAKE15'],
            linesep => ' ', linemax => 1, unescape => 'backslash_whitespace',
            filter => qr/\bzlib\.h\b/, inner => qr/^(.+)\bzlib\.h/,
            output => 'searchpath:systeminclude:',
            verb => 'located',
        },

        find_headers => {
            object => qr/^searchpath:systeminclude:/,
            command => ['intcmd:listtree', 'optpath::', 'optstring:2'],
            filter => qr/\.h$/x,
            output => qr/(.+)/s,
            verb => 'found',
        },

        # We'd like to find libraries the same way we find headers. (Why not
        # simply ask gcc where the libraries are? It has a command-line option
        # to do that, but the output is incorrect, and only useful for finding
        # libgcc.a in particular, as far as I can tell.) However, unlike with
        # the headers, we have lists of relevant libraries in the options
        # already. So to locate them, we run ld (TODO: assuming GNU ld,
        # here...), via the compiler, specifying -Wl,--verbose. This gets the
        # linker to tell us where it found each library. It'll also tell us
        # about the system's libcs, so we don't need to specify those.
        generate_library_test_object_file => {
            object => 'bpath:aimake/aimake_1.c',
            command => ['tool:c_compiler', 'optpath::',
                        "optpath:-o :bpath:aimake/aimake_1$objext"],
            output => "bpath:aimake/aimake_1$objext",
            verb => 'built',
        },
        find_libraries => {
            object => "bpath:aimake/aimake_1$objext",
            command => ['tool:c_linker', 'optpath::',
                        'optionset:libraries',
                        "optpath:-o :bpath:aimake/aimake_1$exeext",
                        'optstring:-Wl,--verbose'],
            # ld doesn't escape its output
            output => qr/^attempt to open (.*) succeeded$/s,
            filter_text_files => 1,
            verb => 'found',
            ignore_failure => 2,
            ignore_warnings => 1,
        },

        # Compiling c.
        c_compiler_tool => [
            {
                output => 'tool:c_compiler',
                outdepends => ['cmd:gcc', 'optionset:CFLAGS', 'optstring:-c'],
                verb => 'found',
            },
        ],
        c_dependencies_tool => [
            {
                output => 'tool:c_dependencies',
                outdepends => ['cmd:gcc', 'optstring:-M -MG',
                               'optionset:IFLAGS'],
                verb => 'found',
            },
        ],
        ch_file_dependencies => {
            object => qr/^b?path:(?!aimake\/).*\.[ch]$/s,
            command => ['tool:c_dependencies', 'optpath::'],
            # A little aimake magic here: this outputs relative paths for
            # non-found files, and those become file_required_by: objects,
            # whereas absolute paths for files it did find will become the
            # appropriate sort of object (most likely path: or spath:).
            linesep => ' ',
            lineskip => 2, # output file, input file
            unescape => 'backslash_whitespace',
            depends => qr/^(.++)$/,
        },
        ch_embedded_aimake_option => {
            object => qr/^b?path:(?!aimake\/).*\.[ch]$/s,
            command => ['intcmd:cat', 'optpath::'],
            depends => 'config_option:',
            dependsarg => qr'AIMAKE_OPTION_([a-zA-Z0-9_]+)',
        },
        embedded_aimake_option_define => {
            object => qr/^config_option:/,
            command => ['intcmd:optionvalues', 'optpath::'],
            depends => 'optstring:-DAIMAKE_OPTION_',
            dependsarg => qr/^(.*)$/,
        },
        ch_embedded_aimake_buildos => {
            # Define AIMAKE_BUILDOS_$^O if it's mentioned by a file.
            # (That is, AIMAKE_BUILDOS_MSWin32 is defined on Windows, etc.)
            object => qr/^b?path:(?!aimake\/).*\.[ch]$/s,
            command => ['intcmd:cat', 'optpath::'],
            depends => "optstring:-DAIMAKE_BUILDOS_",
            dependsarg => qr"AIMAKE_BUILDOS_($^O)",
        },
        headers_on_path_or_bpath => {
            object => qr/^b?path:.*\.h/,
            depends => 'optpath:-I :extend:..:',
        },
        compile_c => {
            object => qr/^b?path:(?!aimake\/).*\.c$/s,
            command => ['tool:c_compiler', 'optpath::',
                        "optpath:-o :extend:$objext:"],
            output => "extend:$objext:",
            verb => 'compiled',
        },

        # Linking.
        c_link_tool => [
            {
                output => 'tool:c_linker',
                outdepends => ['cmd:gcc', 'optionset:LFLAGS'],
                verb => 'found',
            },
        ],
        oa_file_dependencies => {
            # Note that this only matches the bpath, not the spath.
            object => qr/^bpath:.*(?:\Q$objext\E|\Q$libext\E)$/,
            command => ['cmd:nm','optstring:-fp','optpath::'],
            depends => 'symbol_required_by:',
            # To make things work on Windows, omit symbols that start
            # with a double underscore. TODO: This may be a hack, and
            # there may be a better way.
            dependsarg => qr'^((?>_?[a-zA-Z0-9\$][a-zA-Z0-9_\$]*)) U',
            # dependsarg => qr'^((?>[a-zA-Z0-9_\$]*)) U',
        },
        o_file_provisions => {
            object => qr/^b?path:(?!aimake\/).*\Q$objext\E\z/,
            command => ['cmd:nm','optstring:-fp','optpath::'],
            output => 'symbol_in_object:',
            outputarg => qr'^((?>[a-zA-Z0-9_$]+)) [ABCDGRSTVWi]',
            # inner => '' is implied, as with the next few rules
            outdepends => ['', 'optpath::'],
            verb => 'found',
            propagate_usedeps => 1,
        },
        a_file_provisions => {
            object => qr/^s?path:.*\Q$libext\E\z/,
            command => ['cmd:nm','optstring:-fp','optpath::'],
            output => 'symbol_in_object:',
            outputarg => qr'^((?>[a-zA-Z0-9_$]+)) [ABCDGRSTVWi]',
            ignore_warnings => 1,
            outdepends => ['', 'optpath::'], # TODO: -l?
            verb => 'found',
            propagate_usedeps => 1,
        },
        shared_object_provisions_nondynamic => {
            # TODO: condition by platform
            object => qr/^s?path:.*\.(?:so|dll)/,
            command => ['cmd:nm','optstring:-fp','optpath::'],
            output => 'symbol_in_object:',
            outputarg => qr'^((?>[a-zA-Z0-9_$]+)) [ABCDGRSTVWi]',
            ignore_warnings => 1,
            outdepends => ['', 'optpath::'], # TODO: -l?
            verb => 'found',
        },
        shared_object_provisions_dynamic => {
            # TODO: condition by platform
            object => qr/^s?path:.*\.(?:so|dll)/,
            command => ['cmd:nm','optstring:-fp -D','optpath::'],
            output => 'symbol_in_object:',
            outputarg => qr'^((?>[a-zA-Z0-9_$]+)) [ABCDGRSTVWi]',
            ignore_failure => 1,
            ignore_warnings => 1,
            outdepends => ['', 'optpath::'], # TODO: -l?
            verb => 'found',
        },
        link_c => {
            object => qr/^b?path:.*\Q$objext\E/,
            command => ['symbol_in_object:main:', 'tool:c_linker',
                        'optpath::',
                        "optpath:-o :extend:$exeext:"],
            output => "extend:$exeext:",
            verb => 'linked',
        },

        # lex, yacc
        # For the time being we only support the GNU flex/bison, due to
        # traditional lex's nasty habit of using hardcoded filenames.
        lex_tool => [
            {
                output => 'tool:lex',
                outdepends => ['cmd:flex'],
                verb => 'found',
            },
        ],
        compile_lex => {
            object => qr/^b?path:.*\.l$/,
            # Double-extend the filename to avoid potential clashes.
            # Also note the use of -o (with no space) here; GnuWin32
            # flex does not support the --outfile= syntax, so that's
            # the most portable option.
            command => ['tool:lex', 'optpath::',
                        'optpath:-o:extend:_l.c:'],
            output => 'extend:_l.c:',
            verb => 'generated',
        },

        yacc_tool => [
            {
                output => 'tool:yacc',
                outdepends => ['cmd:bison'],
                verb => 'found',
            },
        ],
        compile_yacc => {
            object => qr/^b?path:.*\.y$/,
            # Again, the C file needs extension, in case the project has files
            # file.y and file.l. The header file mustn't have an extension,
            # because it's probably referred to by name.
            command => ['tool:yacc', 'optpath::',
                        'optpath:-o :extend:_y.c:',
                        'optpath:--defines=:extend:.h:'],
            output => ['extend:_y.c:', 'extend:.h:'],
            verb => 'generated',
        },

        # Creating static libraries. We do this by looking at .api
        # files that specify what symbols should be in them. (It'd be
        # nice to somehow mark all the other symbols as nonpublic,
        # but there's no obvious way to do that.)
        ar_tool => [
            {
                output => 'tool:ar',
                outdepends => ['cmd:ar', 'optstring:rcs'],
                verb => 'found',
            },
        ],
        api_dependencies => {
            # bpath is allowed here in case people want to generate .api files
            # at runtime and have them work correctly
            object => qr/^b?path:.*\.api$/,
            command => ['intcmd:cat'],
            filter => qr/^symbol:/,
            depends => 'symbol_required_by:',
            dependsarg => qr/^symbol:([^#]+?)\s*(?:#|$)/,
        },
        create_library => {
            object => qr/^b?path:.*\.api$/,
            command => ['tool:ar', "optpath::extend:$libext:"],
            output => "extend:$libext",
            ignore_spath => 1,
            verb => 'created',
        },
    },
}
