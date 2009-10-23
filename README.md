# 3-MOVE

3-MOVE is a multi-user networked online text-based programmable
virtual environment, designed and built by Tony Garnock-Jones (mostly
during 1997 and 1998). It was originally inspired by Pavel Curtis's
amazing [LambdaMOO](http://en.wikipedia.org/wiki/LambdaMOO) software.

It includes a programming language compiler, virtual machine and
standard libraries intended specifically for building shared,
multi-user object databases and virtual environments. Users of the
system can learn the language and construct new kinds of objects in
the world, scripted with behaviours provided by the users
themselves. A simple extensible adventure-game-like command parser
lets users add to the vocabulary of commands understood by the system.

## Source code layout

There are three directories here:

 - move: contains source code and makefiles for the server.

 - db: contains source code and build scripts to the world
   database. Once you have built the server, you need to build a
   database to run on it.

 - tricks: contains interesting little scripts to install in a world
   database, and a program for cleaning up the periodic database
   checkpoint files that would otherwise litter your hard drive.

## Copyright and licensing

3-MOVE is Copyright 1997, 1998, 1999, 2003, 2005, 2008, 2009 Tony
Garnock-Jones <tonyg@kcbbs.gen.nz>.

3-MOVE is free software: you can redistribute it and/or modify it
under the terms of the GNU Affero General Public License as published
by the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

3-MOVE is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public
License for more details.

You should have received a copy of the GNU Affero General Public
License along with 3-MOVE (it should be available [here](agpl.txt)).
If not, see <http://www.gnu.org/licenses/>.
