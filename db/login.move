// login.move
set-realuid(Wizard);
set-effuid(Wizard);

define Login = Root:clone();
set-owner(Login, Wizard);
set-object-flags(Login, O_OWNER_MASK);

Login:set-name("The Login Object");
move-to(Login, null);

define (Login) players = [Wizard];
define (Login) active-players = [];
set-slot-flags(Login, #players, O_ALL_R | O_OWNER_MASK);
set-slot-flags(Login, #active-players, O_OWNER_MASK);

define (Login) login-message =
"\n"
"	    _/      _/    _/_/_/    _/      _/  _/_/_/_/_/	\n"
"	   _/_/  _/_/  _/      _/    _/  _/    _/		\n"
"	  _/  _/  _/  _/      _/    _/  _/    _/_/_/		\n"
"	 _/      _/  _/      _/      _/      _/			\n"
"	_/      _/    _/_/_/        _/      _/_/_/_/_/		\n"
"\n"
"\n"
;
set-slot-flags(Login, #login-message, O_ALL_R);

define (Login) motd =
"\n"
"Welcome to MOVE."
"\n"
;
set-slot-flags(Login, #motd, O_ALL_R);

{
  define real-GPS = get-print-string;
  get-print-string = function (x) {
    if (type-of(x) == #object)
      "#<object " + x.name + ">";
    else
      real-GPS(x);
  }
  set-setuid(get-print-string, false);
  set-method-flags(get-print-string, O_ALL_PERMS);
}

define function do-login(conn) {
  if (!privileged?(caller-effuid()))	// additional check. Perms should also disallow.
    return false;

  while (!closed?(conn)) {
    print-on(conn, "login: ");
    define lname = read-from(conn);
    print-on(conn, "Password: ");
    define lpass = read-from(conn);

    define player = find-by-name(Login.players, lname);

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

define function match-verb(player, sent) {
  set-effuid(player);
  player:match-verb(sent) || location(player):match-verb(sent);
}

define function match-objects(player, match) {
  set-effuid(player);
  player:match-objects(match);
  location(player):match-objects(match);

  define bindings = match[2];

  for-each(function (key) {
    define v = bindings[key];

    if (type-of(v) == #string)
      bindings[key] = [v, false];
  }, all-keys(bindings));
}

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

define function repl-thread-main(sock) {
  set-connections(sock, sock);
  set-thread-quota(current-thread(), VM_STATE_DAEMON);

  set-realuid(Wizard);
  set-effuid(Wizard);

  define abort = null;
  set-exception-handler(function (excp, arg, vms, acc) {
    realuid():mtell([
		     "EXCEPTION:\n",
		     "  key: ", excp, "\n",
		     "  arg: ", arg, "\n",
		     "  acc: ", acc, "\n",
		     "Aborting to toplevel repl.\n"
    ]);
    abort();
  });
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

  set-realuid(player);
  set-effuid(player);

  player:connect-function();

  make-abort-return-here();
  while (!closed?(sock)) { 
    define sent = read-from(sock);

    if (!closed?(sock)) {
      if (do-mud-command(player, sent) != true)
        player:tell("I don't understand that.\n");
      else
	player:tell("\n");
    }
  }

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
    player:disconnect-function();
  });
}

define function listener-thread-main() {
  define port = 7777;
  define server = listen(port);

  while (type-of(server) != #connection) {
    port = port + 1111;
    server = listen(port);
  }

  while (true) {
    define sock = accept-connection(server);
    fork(function () repl-thread-main(sock));
  }
}

define VM_STATE_DAEMON = -1;	// run forever, checkpoint
define VM_STATE_NOQUOTA = -2;	// run forever, don't save

define function start-listening() {
  define l = fork(listener-thread-main);
  set-thread-quota(l, VM_STATE_NOQUOTA);
}

start-listening();

checkpoint();
shutdown();
.
