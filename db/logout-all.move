// logout-all.move
set-realuid(Wizard);
set-effuid(Wizard);

{
  define function logout1(p) {
    p.awake = false;
    p.connection = null;
  }

  lock(Login);
  for-each(logout1, Login.active-players);
  Login.active-players = [];
  unlock(Login);
}

checkpoint();
shutdown();
.
