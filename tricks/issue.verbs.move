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
// Verbs on the Wizard to deal with setting Login.login-message.

define method (TARGET) @setissue-verb(b) {
  define player = realuid();
  if (player != TARGET) {
    player:tell("You don't have permission to @setissue, I'm sorry.\n");
  } else {
    player:mtell(["Edit the login-message, typing .s to save it or .q to\n",
		 "leave it unchanged.\n"]);
    define m = editor([]);

    if (m == [])
      player:tell("@setissue aborted.\n");
    else {
      Login.login-message = reduce(function (a, x) a + x, "", m);
      player:tell("Login message changed.\n");
    }
  }
}
TARGET:add-verb(#this, #@setissue-verb, ["@setissue"]);
