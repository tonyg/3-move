// registry.move
set-realuid(Wizard);
set-effuid(Wizard);

clone-if-undefined(#Registry, Thing);
set-object-flags(Registry, O_NORMAL_FLAGS);

Registry:set-name("Registry");
Registry:set-description(["The registry of most user-accessible objects in the system.\n"]);

define (Registry) next-number = 1;
set-slot-flags(Registry, #next-number, O_OWNER_MASK);

define (Registry) table-size = 1097;
set-slot-flags(Registry, #table-size, O_OWNER_MASK);

define (Registry) table = make-vector(Registry.table-size, []);
set-slot-flags(Registry, #table, O_OWNER_MASK);

define method (Registry) at(n) {
  define i = n % this.table-size;
  define v = this.table[i];

  first-that(function (elt) elt.registry-number == n, v);
}

define method (Registry) register(x) {
  define n;
 
  if (!slot-clear?(x, #registry-number))
    return x.registry-number;

  lock(this);
  {
    n = this.next-number;
    define i = n % this.table-size;

    this.next-number = n + 1;
    this.table[i] = this.table[i] + [x];
  }
  unlock(this);

  define (x) registry-number = n;
  set-slot-flags(x, #registry-number, O_ALL_R | O_OWNER_MASK);
  n;
}

define method (Registry) deregister(x) {
  if (slot-clear?(x, #registry-number))
    return false;

  if (caller-effuid() != owner(x) && !privileged?(caller-effuid()))
    return false;

  lock(this);
  {
    define n = x.registry-number;
    define i = n % this.table-size;
    define v = this.table[i];

    define idx = index-of(v, x);
    if (!idx) {
      unlock(this);
      return false;
    }
    this.table[i] = delete(v, idx, 1);
  }
  unlock(this);

  clear-slot(x, #registry-number);
  true;
}

define method (Registry) @register-verb(b) {
  define obj = b[#obj][1];

  if (!obj) {
    realuid():tell("@register failed: don't know what that is.\n");
    return;
  }

  realuid():tell("@register: " + get-print-string(obj) + " registered as #" +
		 get-print-string(Registry:register(obj)) + ".\n");
}
Registry:add-verb(#reg, #@register-verb, ["@register ", #obj, " with ", #reg]);

define method (Registry) @deregister-verb(b) {
  define obj = b[#obj][1];

  if (!obj) {
    realuid():tell("@deregister failed: don't know what that is.\n");
    return;
  }

  if (Registry:deregister(obj))
    realuid():tell("@deregister successful.\n");
  else
    realuid():tell("Failed to @deregister.\n");
}
Registry:add-verb(#reg, #@deregister-verb, ["@deregister ", #obj, " from ", #reg]);

define method (Registry) @enumerate-verb(b) {
  define i = 0;
  define acc = ["Registry objects:\n"];

  while (i < this.next-number) {
    define o = this:at(i);
    if (o != undefined)
      acc = acc + ["\t" + get-print-string(i) + "\t" + o:fullname() + "\n"];
    i = i + 1;
  }

  realuid():mtell(acc);
}
Registry:add-verb(#reg, #@enumerate-verb, ["@enumerate ", #reg]);

for-each(Registry:register, [Thing, Player, Room, Exit, Note, Program, Registry, Wizard, Guest]);

checkpoint();
shutdown();
.
