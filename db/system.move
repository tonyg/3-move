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

define function for-each(f, x) {
  define i = 0;

  while (i < length(x)) {
    f(x[i]);
    i = i + 1;
  }
}

define function first-that(f, x) {
  define i = 0;

  while (i < length(x)) {
    if (f(x[i]))
      return x[i];
    i = i + 1;
  }

  return undefined;
}

define function reduce(f, st, vec) {
  define i = 0;

  while (i < length(vec)) {
    st = f(st, vec[i]);
    i = i + 1;
  }

  st;
}

define function find-by-name(vec, name)
     first-that(function (x) equal?(x.name, name), vec);

define function valid(obj) type-of(obj) == #object;

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
      define g = Root.Genders[who.gender];
      define gn = ["%s", "%o", "%p", "%q", "%r"];
      define i = 0;

      if (g != undefined)
	while (i < length(g)) {
	  str = substring-replace(str, gn[i], g[i]);
	  str = substring-replace(str, toupper(gn[i]), cap(g[i]));
	  i = i + 1;
	}
    }
  }

  str;
}

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

    while (!(length(i) > 0 && equal?(i[0], "."))) {
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
	define num2len = length(nums) - ixo - 1;
	define t = length(res);

	if (num2len > 0)
	  t = as-num(section(nums, ixo + 1, num2len));

	if (t < l || t > length(res))
	  p:tell("That line number is invalid.\n");
	else
	  res = section(res, 0, l - 1) + section(res, t, length(res) - t);
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

define function make-sentence(vec) {
  define i = 0;
  define l = length(vec) - 1;
  define sent = "";
  define function onm(x) if (type-of(x) == #object) x.name; else get-print-string(x);

  while (i < l) {
    if (i != (l - 1))
      sent = sent + onm(vec[i]) + ", ";
    else
      sent = sent + onm(vec[i]) + " ";
    i = i + 1;
  }

  if (l > 0) sent + "and " + onm(vec[l]); else onm(vec[l]);
}

define function object-named(name) {
  define player = realuid();
  player:match-object(name) || location(player):match-object(name);
}

checkpoint();
shutdown();
.
