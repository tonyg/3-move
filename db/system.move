// system.move
set-realuid(Wizard);
set-effuid(Wizard);

define function map(f, x) {
  define i = 0;
  define v = make-vector(length(x));

  while (i < length(x)) {
    v[i] = f(x[i]);
    i = i + 1;
  }

  return v;
}
set-setuid(map, false);
set-method-flags(map, O_ALL_PERMS);

define function for-each(f, x) {
  define i = 0;

  while (i < length(x)) {
    f(x[i]);
    i = i + 1;
  }
}
set-setuid(for-each, false);
set-method-flags(for-each, O_ALL_PERMS);

define function first-that(f, x) {
  define i = 0;

  while (i < length(x)) {
    if (f(x[i]))
      return x[i];
    i = i + 1;
  }

  return undefined;
}
set-setuid(first-that, false);
set-method-flags(first-that, O_ALL_PERMS);

define function reduce(f, st, vec) {
  define i = 0;

  while (i < length(vec)) {
    st = f(st, vec[i]);
    i = i + 1;
  }

  st;
}
set-setuid(reduce, false);
set-method-flags(reduce, O_ALL_PERMS);

define function find-by-name(vec, name)
     first-that(function (x) equal?(x.name, name), vec);
set-setuid(find-by-name, false);
set-method-flags(find-by-name, O_ALL_PERMS);

define function valid(obj) type-of(obj) == #object;
set-method-flags(valid, O_ALL_PERMS);

define function pronoun-sub(str, who) {
  define function cap(s) toupper(s[0]) + section(s, 1, length(s) - 1);

  if (valid(who)) {
    str = substring-replace(str, "%n", who.name);
    str = substring-replace(str, "%N", cap(who.name));

    if (valid(location(who))) {
      define loc = location(who);
      str = substring-replace(str, "%l", loc.name);
      str = substring-replace(str, "%L", cap(loc.name));
    }

    if (has-slot(who, #gender)) {
      define g = who.gender;
      define gn = ["%s", "%o", "%p", "%q", "%r"];
      define i = 0;

      while (i < length(g)) {
	str = substring-replace(str, gn[i], g[i]);
	str = substring-replace(str, toupper(gn[i]), toupper(g[i]));
	i = i + 1;
      }
    }
  }

  str;
}
set-setuid(pronoun-sub, false);
set-method-flags(pronoun-sub, O_ALL_PERMS);

// editor(text)
//
// Accepts a vector of strings, and returns the edited vector of strings.
//
define function editor(text) {
  define p = realuid();	// player...

  p:tell("Type .s to save, or .q to lose changes. .? is for help.\n");

  define res = text;
  define inspos = length(res) + 1;

  while (true) {
    define i = p:read();

    while (!equal?(i[0], ".")) {
      if (inspos > length(res))
	res = res + [i + "\n"];
      else
	res = section(res, 0, inspos - 1) + [i + "\n"] +
	  section(res, inspos - 1, length(res) - inspos + 1);
      inspos = inspos + 1;
      i = p:read();
    }

    if (equal?(i, ".s")) return res;
    if (equal?(i, ".q")) return text;

    if (length(i) < 2 || equal?(i[1], "?")) {
      p:tell(
	"Help for the OOM editor:\n"
	"	.s		Save results\n"
	"	.q		Abort, losing your edits\n"
	"	.l		List the current text\n"
	"	.i n		Insert before line n\n"
	"	.d n [, n]	Delete line(s)\n"
	"	.?		This help message\n"
	);
    } else if (equal?(i[1], "i")) {
      inspos = as-num(section(i, 2, length(i) - 2));
      if (inspos < 1) inspos = 1;
    } else if (equal?(i[1], "d")) {
      define nums = section(i, 2, length(i) - 2);
      define l = as-num(nums);
      define ixo = index-of(nums, ",");

      if (l < 1 || l > length(res))
	p:tell("That line number is invalid.\n");
      else if (ixo) {
	define t = as-num(section(nums, ixo + 1, length(nums) - ixo - 1));

	if (t < l || t > length(res))
	  p:tell("That line number is invalid.\n");
	else
	  res = section(res, 0, l - 1) + section(res, t, length(res) - l);
      } else
	res = section(res, 0, l - 1) + section(res, l, length(res) - l);
    } else if (equal?(i[1], "l")) {
      define i = 0;
      define linenumberizer = function (x) {
	i = i + 1;
	get-print-string(i) + "> " + x;
      }

      p:tell("--- Current text:\n");
      p:mtell(map(linenumberizer, res));
      inspos = length(res) + 1;
    } else
      p:tell("Syntax error. .? is for help.\n");
  }
}
set-setuid(editor, false);
set-method-flags(editor, O_ALL_PERMS);

checkpoint();
shutdown();
.
