// root.move

define O_C_FLAG		= 0x00004000;
define O_PERMS_MASK	= 0x00000FFF;
define O_ALL_PERMS	= O_PERMS_MASK;

define O_OWNER_MASK	= 0x00000F00;
define O_GROUP_MASK	= 0x000000F0;
define O_WORLD_MASK	= 0x0000000F;
define O_ALL_R		= 0x00000111;
define O_ALL_W		= 0x00000222;
define O_ALL_X		= 0x00000444;

define O_OWNER_R	= 0x00000100;
define O_OWNER_W	= 0x00000200;
define O_OWNER_X	= 0x00000400;
define O_GROUP_R	= 0x00000010;
define O_GROUP_W	= 0x00000020;
define O_GROUP_X	= 0x00000040;
define O_WORLD_R	= 0x00000001;
define O_WORLD_W	= 0x00000002;
define O_WORLD_X	= 0x00000004;

define O_NORMAL_FLAGS	= O_ALL_R | O_ALL_X | O_OWNER_MASK;

define Wizard = clone(Root);
set-privileged(Wizard, true);
set-owner(Wizard, Wizard);
set-owner(Root, Wizard);
set-realuid(Wizard);
set-effuid(Wizard);
set-object-flags(Root, O_NORMAL_FLAGS);
set-object-flags(Wizard, O_OWNER_MASK);	// wizard is a very private individual.

define (Root) name = "The Root Object";
set-slot-flags(Root, #name, O_ALL_R);

define method (Root) set-name(n) {
  define c = caller-effuid();

  if (c == owner(this) || privileged?(c))
    this.name = n;
  else
    false;
}
set-method-flags(Root:set-name, O_ALL_PERMS);

Wizard:set-name("Wizard");

move-to(Root, null);
move-to(Wizard, null);

{
  define real-clone = clone;
  clone = null;

  define method (Root) clone() {
    define n = real-clone(this);
    if (n)
      n:initialize();
    n;
  }
  set-setuid(Root:clone, false);
  set-method-flags(Root:clone, O_ALL_PERMS);
}

define method (Root) initialize() true;
set-method-flags(Root:initialize, O_ALL_PERMS | O_C_FLAG);

define function all-methods(obj) {
  define p = parents(obj);
  define i = 0;
  define result = [];

  while (i < length(p)) {
    result = result + all-methods(p[i]);
    i = i + 1;
  }

  return methods(obj) + result;
}
set-setuid(all-methods, false);

checkpoint();
shutdown();
.
