// A HTTP server for MOVE.
// Target this at the server object.
// You have to be wizard to compile this code.

define (TARGET) server-thread = -1;
set-slot-flags(TARGET, #server-thread, O_OWNER_MASK);

define method (TARGET) start-server(port) {
  this:stop-server();

  define sock = listen(port);

  if (type-of(sock) != #connection)
    return false;

  define server-fn = function () {
    define conn = accept-connection(sock);
    this.server-thread = fork(server-fn);

    define line = read-from(conn);
    define keyw = section(line, 0, index-of(line, " "));
    define arg = section(line, index-of(line, " ") + 1, -1);

    {
      define i = index-of(arg, " ");
      if (i)
	arg = section(arg, 0, i);
    }

    define onum = as-num(section(arg, 1, -1));
    define tag = {
      define i = index-of(arg, "?");
      if (i) as-sym(section(arg, i + 1, -1)); else #look;
    }

    if (!onum || Registry:at(onum) == undefined) {
      for-each(function (line) print-on(conn, line), [
"404 Not Found<p>\n",
"That object doesn't exist.\n"
      ]);
    } else {
      define ooo = Registry:at(onum);
      if (!has-method(ooo, #http-dispatch)) {
	for-each(function (line) print-on(conn, line), [
"400 Bad request<p>\n",
"That object doesn't have an http dispatcher.\n"
	]);
      } else {
        ooo:http-dispatch(conn, tag);
      }
    }

    close(conn);
  }

  fork(server-fn);
}

define method (TARGET) stop-server() {
  if (this.server-thread != -1) {
    kill(this.server-thread, #die, null);
    this.server-thread = -1;
  }
}

define method (TARGET) @start-verb(b) {
  define t = b[#port][0];
  define n = as-num(t);

  if (!privileged?(caller-effuid())) {
    realuid():tell("You don't have permissions to @start that server.\n");
    return;
  }

  if (!n) {
    realuid():tell("I don't understand that port number.\n");
  } else {
    define th = this:start-server(n);
    if (th)
      realuid():tell("You started a server on port " + get-print-string(n) +
		     " with thread number " + get-print-string(th) + ".\n");
    else
      realuid():tell("You failed to start a server on that port.\n");
  }
}
TARGET:add-verb(#server, #@start-verb, ["@start ", #server, " on ", #port]);

define method (TARGET) @stop-verb(b) {
  if (!privileged?(caller-effuid())) {
    realuid():tell("You don't have permissions to @stop that server.\n");
    return;
  }

  this:stop-server();
  realuid():tell("The server has been sent a stop request.\n");
}
TARGET:add-verb(#server, #@stop-verb, ["@stop ", #server]);

define method (Thing) http-dispatch(conn, tag) {
  if (tag == #look) {
    print-on(conn,
	     reduce(function (acc, l) acc + l, "<pre>\n", this:look()) +
	     "\n</pre>\n");
  } else if (tag == #examine) {
    print-on(conn,
	     reduce(function (acc, l) acc + l, "<pre>\n", this:examine()) +
	     "\n</pre>\n");
  } else {
    print-on(conn, "Unknown tag...<p>\n");
  }
}
make-method-overridable(Thing:http-dispatch, true);

