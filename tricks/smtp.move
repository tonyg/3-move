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
