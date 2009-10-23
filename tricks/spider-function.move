// 3-MOVE, a multi-user networked online text-based programmable virtual environment
// Copyright 1997, 1998, 1999, 2003, 2005, 2008, 2009 Tony Garnock-Jones <tonyg@kcbbs.gen.nz>
//
// 3-MOVE is free software: you can redistribute it and/or modify it
// under the terms of the GNU Affero General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// 3-MOVE is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public
// License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with 3-MOVE.  If not, see <http://www.gnu.org/licenses/>.
//
// Spider that registers all accessible rooms and their contents.

{
  define ptell = realuid():tell;

  define function regspider(o, stk) {
    define function already-met?(x, stk) {
      if (stk == null)
	return false;
      if (x == stk[0])
	return true;
      return already-met?(x, stk[1]);
    }

    if (slot-clear?(o, #registry-number)) {
      Registry:register(o);
      ptell("Registered " + o:fullname() + "\n");
    }

    stk = [o, stk];
    define function f(x) if (!already-met?(x, stk)) regspider(x, stk);

    for-each(f, contents(o));
    if (isa(o, Room))
      for-each(function (x) f(x.dest), o.exits);
  }

  fork/quota(function () {
    regspider(location(realuid()), null);
    ptell("Done.\n");
  }, -2);
}
