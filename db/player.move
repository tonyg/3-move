// player.move
set-realuid(Wizard);
set-effuid(Wizard);

define Player = Thing:clone();
set-object-flags(Player, O_NORMAL_FLAGS);
move-to(Player, null);

define method (Player) clone() {
  if (!privileged?(effuid()))
    undefined;
  else
    as(Thing):clone();
}
set-setuid(Player:clone, false);
set-method-flags(Player:clone, O_ALL_PERMS);

define method (Player) set-name(n) {
  if (!privileged?(caller-effuid()))
    false;
  else
    as(Thing):set-name(n);
}
set-method-flags(Player:set-name, O_ALL_PERMS);

Player:set-name("Generic Player");

define (Player) password = "";
set-slot-flags(Player, #password, O_OWNER_MASK);

define method (Player) set-password(newpass) {
  if (caller-effuid() != owner(this) && !privileged?(caller-effuid()))
    return false;

  this.password = newpass;
  true;
}
set-method-flags(Player:set-password, O_ALL_PERMS);

define (Player) connection = null;
set-slot-flags(Player, #connection, O_ALL_R);

define (Player) awake = false;
set-slot-flags(Player, #awake, O_ALL_R);

define (Player) home = null;
set-slot-flags(Player, #home, O_ALL_R);

define method (Player) set-home(newhome) {
  if (caller-effuid() != owner(this) && !privileged?(caller-effuid()))
    return false;

  this.homd = newhome;
  true;
}
set-method-flags(Player:set-home, O_ALL_PERMS);

define (Player) is-programmer = false;
set-slot-flags(Player, #is-programmer, O_ALL_R);

define method (Player) set-programmer(v) {
  if (caller-effuid() != owner(this) && !privileged?(caller-effuid()))
    return false;

  this.is-programmer = v;
  true;
}
set-method-flags(Player:set-programmer, O_ALL_PERMS);

define method (Player) connect-function() {
  this:moveto(this.home);
}
set-setuid(Player:connect-function, false);
set-method-flags(Player:connect-function, O_ALL_PERMS | O_C_FLAG);

define method (Player) disconnect-function() {
  this:moveto(this.home);
}
set-setuid(Player:disconnect-function, false);
set-method-flags(Player:disconnect-function, O_ALL_PERMS | O_C_FLAG);

define method (Player) match-object(name) {
  if (equal?(name, "me"))
    return this;

  return as(Thing):match-object(name);
}
set-method-flags(Player:match-object, O_ALL_PERMS | O_C_FLAG);

define method (Player) read() {
  if (type-of(this.connection) == #connection)
    read-from(this.connection);
  else
    "";
}
set-method-flags(Player:read, O_ALL_PERMS);

define method (Player) tell(x) {
  if (this:listening?())
    print-on(this.connection, get-print-string(x));
}
set-method-flags(Player:tell, O_ALL_PERMS);

define method (Player) listening?()
  this.awake && type-of(this.connection) == #connection;
set-method-flags(Player:listening?, O_ALL_PERMS);

set-parents(Wizard, Player);
Wizard:initialize();
Wizard.password = "wiz";
Wizard.is-programmer = true;

checkpoint();
shutdown();
.
