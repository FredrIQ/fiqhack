" NetHack4 date conflict resolver vim script
" Last modified by Sean Hunt, 2014-08-25
" Copyright (c) Sean Hunt, 2014
" This script may be freely redistributed under the same license as NetHack.
" See license for details.

function! <SID>resolvediff()
  if getline(2) =~ "^<<<<<<<" && getline(3) =~ "Last modified"
    if getline(4) =~ "^=======" && getline(6) =~ "^>>>>>>>"
      2d
      3,5d
    elseif getline(4) =~ "^|||||||" && getline(6) =~ "^=======" &&
           \ getline(8) =~ "^>>>>>>>"
      2d
      3,7d
    endif
    call cursor(0, 0)
    if search("^<<<<<<")
      let @/ = "^<<<<<<<"
    else
      w
      !git add %
    endif
  elseif search("^<<<<<<<", "w")
    let @/ = "^<<<<<<"
  endif
endf

augroup DateConflictResolve
  au!
  au BufReadPost * call <SID>resolvediff()
augroup END
