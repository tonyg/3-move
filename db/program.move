// program.move
set-realuid(Wizard);
set-effuid(Wizard);

clone-if-undefined(#Program, Note);
set-object-flags(Program, O_NORMAL_FLAGS | O_C_FLAG);

Program:set-name("Generic Program");
Program:set-description(["A small piece of paper, with technical-looking marks on it.\n"]);
//Program:add-alias("program");

define (Program) target = null;
set-slot-flags(Program, #target, O_ALL_R | O_OWNER_MASK);

define method (Program) examine() {
  as(Note):examine() + "The target of this program is " + get-print-string(this.target) + ".\n";
}
make-method-overridable(Program:examine, true);

define method (Program) @target-verb(b) {
  define ptell = realuid():tell;

  if (caller-effuid() != owner(this)) {
    ptell("You cannot @target this program, because you don't own it!\n");
    return;
  }

  define tgt = b[#target][1];

  if (!tgt) {
    ptell("That target (\"" + b[#target][0] + "\") is not an object I recognise.\n");
    return;
  }

  this.target = tgt;
  ptell("You target " + this:fullname() + " at " + tgt:fullname() + ".\n");
}
Program:add-verb(#program, #@target-verb, ["@target ", #program, " at ", #target]);

define method (Program) @compile-verb(b) {
  if (effuid() != owner(this)) {
    realuid():tell("You cannot @compile this program, because you don't own it!\n");
    return;
  }

  define p = reduce(function (a, l) a + l, "", this:text());

  p = "function (TARGET) { \n" + p + "}\n";

  define thunk = compile(p);

  if (!thunk) {
    realuid():tell("There was an error during compilation.\n");
    return;
  }

  define targetable = thunk();
  define result = targetable(this.target);
  realuid():tell("Result: " + get-print-string(result) + "\n");
}
set-setuid(Program:@compile-verb, false);
Program:add-verb(#program, #@compile-verb, ["@compile ", #program]);

checkpoint();
shutdown();
.
