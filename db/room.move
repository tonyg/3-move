// room.move
set-realuid(Wizard);
set-effuid(Wizard);

define Room = Thing:clone();
Room:set-name("Generic Room");
move-to(Room, null);

Player.home = Room;

Room:set-description([
		      "This room is the model for all other rooms in the universe.\n",
		      "It is bare, painted pale green, cubical, and is illuminated\n",
		      "by a single unshaded electric bulb hanging from a short length\n",
		      "of cable from the centre of the ceiling.\n"
]);

define method (Room) pre-accept(x) true;
set-method-flags(Room:pre-accept, O_ALL_PERMS | O_C_FLAG);

Room:add-verb(#this, #look-verb, ["look"]);

define method (Room) match-object(name) {
  if (equal?(name, "here"))
    return this;

  return as(Thing):match-object(name);
}
set-method-flags(Room:match-object, O_ALL_PERMS | O_C_FLAG);

Player.home = Room;

checkpoint();
shutdown();
.
