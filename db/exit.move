// exit.move
set-realuid(Wizard);
set-effuid(Wizard);

define Exit = Thing:clone();
set-object-flags(Exit, O_NORMAL_FLAGS | O_C_FLAG);

Exit:set-name("Generic Exit");
Exit:set-description(["Nothing special, just a door.\n"]);

define (Exit) dest = null;
set-slot-flags(Exit, #dest, O_ALL_R | O_OWNER_MASK | O_C_FLAG);

define (Exit) open = true;
set-slot-flags(Exit, #open, O_ALL_R | O_OWNER_MASK | O_C_FLAG);

checkpoint();
shutdown();
.
