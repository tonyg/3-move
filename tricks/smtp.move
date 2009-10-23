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
// A SMTP program for MOVE.
// Target this at the client object.
// You have to be wizard to compile this.

define method (TARGET) send-message(addr, from, msgtext) {
  define sock = open("localhost", 25);

  call/cc(function (exitwith) {
    define check-drop = function(code) {
      if (substring-search(read-from(sock), code) != 0) {
	close(sock);
	exitwith(false);
      }
    }

    check-drop("220");
    print-on(sock, "HELO movedb\n");
    check-drop("250");
    print-on(sock, "MAIL From: " + from + "\n");
    check-drop("250");
    print-on(sock, "RCPT To: " + addr + "\n");
    check-drop("250");
    print-on(sock, "DATA\n");
    check-drop("35");
    for-each(function (line) print-on(sock, line), msgtext);
    print-on(sock, ".\n");
    check-drop("250");
    print-on(sock, "QUIT\n");
    check-drop("221");
    close(sock);
    exitwith(true);
  });
}

define method (TARGET) sendto-verb(b) {
  define addr = b[#addr][0];
  define msgtext = editor([]);

  if (this:send-message(addr, "moveMail", msgtext))
    realuid():tell("Message sent successfully.\n");
  else
    realuid():tell("Message was not sent successfully.\n");
}
TARGET:add-verb(#client, #sendto-verb, ["email ", #addr, " using ", #client]);
