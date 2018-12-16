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
// login.move
set-realuid(Wizard);
set-effuid(Wizard);

if (Login == undefined)
  define Login = Root:clone();
else
  strip-object-methods(Login);
set-owner(Login, Wizard);
set-object-flags(Login, O_OWNER_MASK);

Login:set-name("The Login Object");
move-to(Login, null);

{
  if (!has-slot(Login, #players))
    define (Login) players = [];
  if (!has-slot(Login, #active-players))
    define (Login) active-players = [];
  set-slot-flags(Login, #players, O_OWNER_MASK);
  set-slot-flags(Login, #active-players, O_OWNER_MASK);
}

{
  if (!has-slot(Login, #guests))
    define (Login) guests = [];
  set-slot-flags(Login, #guests, O_OWNER_MASK);
}

{
  if (!has-slot(Login, #login-message))
    define (Login) login-message =
"\n"
"	    _/      _/    _/_/_/    _/      _/  _/_/_/_/_/	\n"
"	   _/_/  _/_/  _/      _/    _/  _/    _/		\n"
"	  _/  _/  _/  _/      _/    _/  _/    _/_/_/		\n"
"	 _/      _/  _/      _/      _/      _/			\n"
"	_/      _/    _/_/_/        _/      _/_/_/_/_/		\n"
"\n"
"3-MOVE Copyright (C) 1997--2009 Tony Garnock-Jones.\n"
"This program comes with ABSOLUTELY NO WARRANTY; for details see\n"
"https://leastfixedpoint.com/tonyg/kcbbs/projects/3-move.html.\n"
"This is free software, and you are welcome to redistribute it\n"
"under certain conditions; see http://github.com/tonyg/3-move for\n"
"details.\n"
"\n"
"\n"
;
}
set-slot-flags(Login, #login-message, O_ALL_R);

{
  if (!has-slot(Login, #motd))
    define (Login) motd =
"\n"
"Welcome to MOVE."
"\n"
;
}
set-slot-flags(Login, #motd, O_ALL_R);

{
  if (type-of(get-print-string != #closure)) {
    define real-GPS = get-print-string;
    get-print-string = function (x) {
      if (type-of(x) == #object)
	"#<object " + x:fullname() + ">";
      else
	real-GPS(x);
    }
  }
}

define method (Login) add-player(p) {
  if (!privileged?(caller-effuid()))
    return false;

  lock(this);
  if (index-of(this.players, p) == false)
    this.players = this.players + [p];
  unlock(this);
  return true;
}

define method (Login) del-player(p) {
  if (!privileged?(caller-effuid()))
    return false;

  lock(this);
  this.players = remove(this.players, p);
  unlock(this);
  return true;
}

define method (Login) add-guest(p) {
  if (!privileged?(caller-effuid()))
    return false;

  lock(this);
  if (index-of(this.guests, p) == false)
    this.guests = this.guests + [p];
  unlock(this);
  return true;
}

define method (Login) del-guest(p) {
  if (!privileged?(caller-effuid()))
    return false;

  lock(this);
  this.guests = remove(this.guests, p);
  unlock(this);
  return true;
}

define function do-login(conn) {
  if (!privileged?(caller-effuid()))	// additional check. Perms should also disallow.
    return false;

  while (!closed?(conn)) {
    print-on(conn, "login: ");
    define lname = read-from(conn);
    define lpass = "";
    if (!equal?(lname, "Guest")) {
      print-on(conn, "Password: ");
      lpass = read-from(conn);
    }

    lock(Login);
    define player = undefined;
    if (lname) {
      if (equal?(lname, "Guest")) {
	define g = first-that(function (g) !g.awake, Login.guests);
	if (g == undefined)
	  print-on(conn,
		   "\n"
		   "All guest characters are in use at the moment.\n"
		   "Try again later.\n"
		   );
	player = g;
      } else
	player = find-by-name(Login.players, lname);
    }
    unlock(Login);

    if (player == undefined) {
      print-on(conn, "\nUnknown player or incorrect password.\n\n");
    } else {
      if (equal?(player.password, lpass)) {
	print-on(conn, "\nLogging in as player " + player.name + "...\n");
	lock(Login);
	if (index-of(Login.active-players, player)) {
	  unlock(Login);
	  print-on(conn, "\nPlayer already connected.\n\n");
	} else {
	  Login.active-players = Login.active-players + [player];
	  unlock(Login);
	  return player;
	}
      } else {
	print-on(conn, "\nUnknown player or incorrect password.\n\n");
      }
    }
  }

  return false;
}
set-method-flags(do-login, O_OWNER_MASK);

define function match-verb(player, sent) {
  define function match-contents-of(x) {
    define res;
    define ob = first-that(function (co) res = co:match-verb(sent), contents(x));
    if (ob != undefined)
      return res;
    else
      return false;
  }

  define function match-hashes(s) {
    define idx = index-of(s, "#");

    if (idx) {
      define rest = section(s, idx + 1, -1);
      define n = as-num(rest);

      if (n) {
	define rego = Registry:at(n);
	if (rego != undefined) {
	  define m = rego:match-verb(sent);
	  if (m)
	    return m;
	}
      }

      match-hashes(rest);
    }

    false;
  }

  set-effuid(player);
  match-hashes(sent) ||
    player:match-verb(sent) ||
    match-contents-of(player) ||
    location(player):match-verb(sent) ||
    match-contents-of(location(player));
}
set-method-flags(match-verb, O_OWNER_MASK);

define function match-objects(player, match) {
  set-effuid(player);
  player:match-objects(match);
  location(player):match-objects(match);

  define bindings = match[2];

  for-each(function (key) {
    define v = bindings[key];

    if (type-of(v) == #string) {
      if (length(v) > 0 && equal?(section(v, 0, 1), "#")) {
	define n = as-num(section(v, 1, -1));
	if (n) {
	  define rego = Registry:at(n);
	  if (rego != undefined) {
	    bindings[key] = [v, rego];
	    return;
	  }
	}
      }

      bindings[key] = [v, false];
    }
  }, all-keys(bindings));
}
set-method-flags(match-objects, O_OWNER_MASK);

define function do-mud-command(player, sent) {	// sent is the Sentence (string).
  if (equal?(sent, ""))		// "understands" the null sentence.
    return true;

  if (equal?(sent[0], "\"")) {
    sent = "say " + section(sent, 1, length(sent) - 1);
  } else if (equal?(sent[0], ":")) {
    sent = "emote" + section(sent, 1, length(sent) - 1);
  } else if (equal?(sent, ".program") && player.is-programmer) {
    set-effuid(player);

    define text = editor([]);

    if (length(text) > 0) {
      define str = reduce(function (a, b) a + b, "", text);
      define thunk = compile(str);
      define result = "Compiler Returned Error Condition";

      if (thunk)
	result = thunk();

      player:mtell(["--> ", result, "\n"]);
    } else
      player:tell("Program aborted.\n");

    return true;
  }

  define match = match-verb(player, sent);
  define loc = location(player);

  if (match) {
    match-objects(player, match);
    set-effuid(player);

    define selfvar = match[0];
    define methname = match[1];
    define bindings = match[2];

    define the-self = bindings[selfvar][1];

    if (!valid(the-self))
      match[3](#continue);	// continues the search.

    define the-meth = method-ref(the-self, methname);

    if (the-meth == undefined)
      match[3](#continue);	// continues the search.

    the-meth(bindings);
    return true;
  } else if (valid(loc) && has-method(loc, #huh?)) {
    set-effuid(player);
    loc:huh?(sent);
    true;
  } else
    #no-matches-found;
}
set-method-flags(do-mud-command, O_OWNER_MASK);

define function repl-thread-main(sock) {
  // set-connections(sock, sock);

  set-realuid(Wizard);
  set-effuid(Wizard);

  define abort = null;
  define function excp-handler(excp, arg, vms, acc) {
    set-thread-quota(current-thread(), VM_STATE_DAEMON); // jic we ate all our quota.
    if (excp == #die)
      return;	// kills the thread.
    if (excp == #quota-expired)
      set-exception-handler(excp-handler);
    if (type-of(arg) == #vector)
      arg = map(get-print-string, arg);
    realuid():mtell([
		     "EXCEPTION:\n",
		     "  key: ", excp, "\n",
		     "  arg: ", arg, "\n",
		     "  acc: ", acc, "\n",
		     "Aborting to toplevel repl.\n"
    ]);
    abort();
  }
  set-setuid(excp-handler, true);
  set-exception-handler(excp-handler);
  define function make-abort-return-here() {
    call/cc(function (cc) {
      abort = function () cc(undefined);
    });
  }

  make-abort-return-here();
  print-on(sock, Login.login-message);

  define player = do-login(sock);
  if (!player)
    return;

  print-on(sock, Login.motd);

  player.awake = true;
  player.connection = sock;

  define function do-mud-command-shell(p, s) {
    set-thread-quota(current-thread(), 100000);	// enough?? we'll see.
    do-mud-command(p, s);
  }
  set-setuid(do-mud-command-shell, true);	// a-ha!

  define function logout-shell() {
    lock(Login);
    {
      define i = index-of(Login.active-players, player);
      if (i)
	Login.active-players = delete(Login.active-players, i, 1);
    }
    unlock(Login);

    player.awake = false;
    player.connection = null;

    call/cc(function (cc) {
      abort = function () cc(undefined);
      set-effuid(player);
      player:disconnect-function();
    });
  }
  set-setuid(logout-shell, true);	// a-ha also!

  set-realuid(player);
  set-effuid(player);

  player:connect-function();

  make-abort-return-here();
  while (!closed?(sock)) { 
    define sent = read-from(sock);

    if (!closed?(sock)) {
      if (do-mud-command-shell(player, sent) != true)
        player:tell("I don't understand that.\n");
      else
	player:tell("\n");
    }
  }

  logout-shell();
}
set-method-flags(repl-thread-main, O_OWNER_MASK);

define function listener-thread-main() {
  define port = 7777;
  define server = listen(port);

  while (type-of(server) != #connection) {
    port = port + 1111;
    server = listen(port);
  }

  while (true) {
    define sock = accept-connection(server);
    fork/quota(function () repl-thread-main(sock), VM_STATE_DAEMON);
  }
}
set-method-flags(listener-thread-main, O_OWNER_MASK);

define VM_STATE_DAEMON = -1;	// run forever, checkpoint
define VM_STATE_NOQUOTA = -2;	// run forever, don't save

define function start-listening() {
  fork/quota(listener-thread-main, VM_STATE_NOQUOTA);
}
set-method-flags(start-listening, O_OWNER_MASK);

start-listening();

checkpoint();
shutdown();
.
