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
