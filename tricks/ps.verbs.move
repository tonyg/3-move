// Verbs on the Wizard for @ps

define method (TARGET) @ps-verb(b) {
  define player = realuid();
  if (player != TARGET) {
    player:tell("You don't have permission to @ps, I'm sorry.\n");
  } else {
    define tab = get-thread-table();

    player:mtell(["Process table:\n"] + map(function(p) {
      " #" + get-print-string(p[0]) + "\t" +
	p[1].name + "\t\t" +
	get-print-string(p[2]) + "\t" +
	get-print-string(p[3]) + "\n";
    }, tab));
  }
}
TARGET:add-verb(#this, #@ps-verb, ["@ps"]);
