# vimtree
A small program for editing files with vim in a tree

# Building
[Linux](https://github.com/gdsonicpython/vimtree#linux)

[Windows](https://github.com/gdsonicpython/vimtree#windows) (semi-supported)

[MacOS](https://github.com/gdsonicpython/vimtree#macos) (To be finished by someone that actually has a Mac)

## Linux
``` bash
g++ -std=c++17 -O2 -Wno-unused-result -o vimtree vimtree.cpp -lncurses
``` 

## Windows
### Prerequisites
- MinGW (llvm-mingw via WinGet)
- PDCurses (https://github.com/wmcbrine/PDCurses)
### Clone PDCurses
```cmd
git clone https://github.com/wmcbrine/PDCurses.git C:\temp\PDCurses
```

### Create ncurses.h symlink
```cmd
copy C:\temp\PDCurses\curses.h C:\temp\PDCurses\ncurses.h
```

### Compile
Note: Your llvm-mingw path might vary based on the version installed via WinGet, I'd do a ``which`` (if you have msys)
```cmd
set GXX=C:\Users\<user>\AppData\Local\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW.MSVCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\llvm-mingw-20260421-msvcrt-x86_64\bin\g++.exe

REM Compile pdcurses core
"%GXX%" -c -O2 -x c -IC:\temp\PDCurses C:\temp\PDCurses\pdcurses\*.c

REM Compile wincon port
"%GXX%" -c -O2 -x c -IC:\temp\PDCurses C:\temp\PDCurses\wincon\*.c

REM Link vimtree
"%GXX%" -std=c++17 -O2 -o vimtree.exe vimtree.cpp -IC:\temp\PDCurses *.o
```

### Optional: Clean up object files
```cmd
del *.o
```

## MacOS
macos isnt real, so you're on your own

<sub>Jokes aside, I don't have a Mac to figure out how to compile it, sorry, if you can compile it, open a PR and put the instructions.</sub>
