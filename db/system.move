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

define function remove(v, elt) {
  define i = index-of(v, elt);

  if (i)
    delete(v, i, 1);
  else
    v;
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
     first-that(function (x) equal?(x.name, name) ||
			     (length(name) > 0 &&
			      !slot-clear?(x, #registry-number) &&
			      as-num(section(name, 1, -1)) == x.registry-number),
		vec);

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
set-setuid(pronoun-sub, true);

// editor(text)
//
// Accepts a vector of strings, and returns the edited vector of strings.
//
define function editor(text) {
  define p = realuid();	// player...
  define ptell = p:tell;
  define pread = p:read;
  define pinvline = function () ptell("That line number is invalid.\n");

  ptell("Type .s to save, or .q to lose changes. .? is for help.\n");

  define res = text;
  define inspos = length(res) + 1;

  define function get-range(text, require?, resvec) {
    define lo = 1;
    define hi = length(resvec);

    text = section(text, 2, length(text) - 2);

    define v1 = as-num(text);
    define v2 = v1;
    define ixo = index-of(text, ",");

    if (!v1) {
      if (require? || ixo)
	return false;		// no range given or invalid syntax
      else
	return #no-range;	// no range given, but that's okay
    }

    if (v1 < lo || v1 > hi)
      return false;		// the range is invalid

    if (ixo) {
      define num2len = length(text) - ixo - 1;

      if (num2len > 0) {
	v2 = as-num(section(text, ixo + 1, num2len));
	if (v2 < v1 || v2 > hi)
	  return false;		// the range is invalid
      } else
	v2 = hi;
    }

    [v1, v2];			// the range is okay.
  }

  while (true) {
    if (inspos > length(res) + 1)
      inspos = length(res) + 1;
    ptell(get-print-string(inspos) + "> ");
    define i = pread();

    while (!(length(i) > 0 && equal?(i[0], "."))) {
      if (inspos > length(res))
	res = res + [i + "\n"];
      else
	res = section(res, 0, inspos - 1) + [i + "\n"] +
	  section(res, inspos - 1, length(res) - inspos + 1);
      inspos = inspos + 1;
      ptell(get-print-string(inspos) + "> ");
      i = p:read();
    }

    if (equal?(i, ".s")) return res;
    if (equal?(i, ".q")) return text;

    if (length(i) < 2 || equal?(i[1], "?")) {
      ptell(
	"Help for the OOM editor:\n"
	"	.s			Save results\n"
	"	.q			Abort, losing your edits\n"
	"	.l n [, n]		List the current text\n"
	"	.i n			Insert before line n\n"
	"	.d n [, n]		Delete line(s)\n"
	"	.f [n, n]		Find text in range\n"
	"	.r [n, n]		Replace text in range\n"
	"	.?			This help message\n"
	);
    } else if (equal?(i[1], "i")) {
      define r = get-range(i, true, res);
      if (!r)
	pinvline();
      else
	inspos = r[0];
    } else if (equal?(i[1], "d")) {
      define r = get-range(i, true, res);

      if (!r)
	pinvline();
      else {
	if (!r[1])
	  r[1] = r[0];

	res = section(res, 0, r[0] - 1) + section(res, r[1], length(res) - r[1]);
      }
    } else if (equal?(i[1], "l")) {
      define r = get-range(i, false, res);
      define i = 1;
      define top = length(res);
      define linenumberizer = function (x) {
	define line = get-print-string(i) + "> " + x;
	i = i + 1;
	line;
      }

      if (!r)
	pinvline();
      else {
	if (r != #no-range) {
	  i = r[0];
	  if (r[1])
	    top = r[1];
	}

	ptell("--- Current text:\n");
        p:mtell(map(linenumberizer, section(res, i - 1, (top - i) + 1)));
	inspos = top + 1;
      }
    } else if (equal?(i[1], "f")) {
      define r = get-range(i, false, res);
      define i = 1;
      define top = length(res);

      if (!r)
	pinvline();
      else {
	if (r != #no-range) {
	  i = r[0];
	  if (r[1])
	    top = r[1];
	}

	ptell("Search for what? :");
	define what = pread();

	define sres = [];
	for-each(function (line) {
	  if (substring-search(line, what))
	    sres = sres + [get-print-string(i) + "> " + line];
	  i = i + 1;
	}, section(res, i - 1, (top - i) + 1));

	ptell("--- Search results:\n");
	if (length(sres) == 0)
	  ptell("*** String not found.\n");
	else
	  p:mtell(sres);
      }
    } else if (equal?(i[1], "r")) {
      define r = get-range(i, false, res);
      define i = 1;
      define top = length(res);

      if (!r)
	pinvline();
      else {
	if (r != #no-range) {
	  i = r[0];
	  if (r[1])
	    top = r[1];
	}

	ptell("Search for what? : ");
	define what = pread();

	ptell("Replace with what? : ");
	define with = pread();

	define ask? = true;
	bind-cc (cancel)
	  res =
	    section(res, 0, i - 1) +
	    map(function (line) {
	      if (substring-search(line, what)) {
		if (ask? == true) {
		  ptell(get-print-string(i) + "> " + line);
		  ptell("Replace? (y)es, (n)o, (cancel), yes to (a)ll, no to (r)emainder: ");

		  define resp = pread();

		  if (equal?(resp, "cancel"))
		    cancel(#f);

		  if (equal?(resp, "r"))
		    ask? = #no;

		  if (equal?(resp, "a")) {
		    ask? = false;
		    resp = "y";
		  }

		  if (equal?(resp, "y"))
		    line = substring-replace(line, what, with);
		} else if (ask? == false) {
		  line = substring-replace(line, what, with);
		}
	      }
	      i = i + 1;
	      line;
	    }, section(res, i - 1, (top - i) + 1)) +
	    section(res, top, length(res) - top);

	ptell("--- Search and replace complete.\n");
      }
    } else
      ptell("Syntax error. .? is for help.\n");
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
  if (length(name) > 0 && equal?(section(name, 0, 1), "#")) {
    define n = as-num(section(name, 1, -1));
    if (n) {
      define rego = Registry:at(n);
      if (rego != undefined)
	return rego;
    }
  }

  define player = realuid();
  player:match-object(name) || location(player):match-object(name);
}

define function vector-equal?(v1, v2) {
  define i = 0;
  define len = length(v1);

  if (len != length(v2))
    return false;

  while (i < len) {
    if (!equal?(v1[i], v2[i]))
      return false;

    i = i + 1;
  }

  true;
}

define function wrap-string(str, width) {
  if (width == 0)
    width = realuid().screen-width;

  if (length(str) <= width)
    return [str];

  define i = width - 1;
  define minw = (width * 7) / 8;

  while (i > minw) {
    if (equal?(str[i], " "))
      return [section(str, 0, i) + "\n"] +
	wrap-string(section(str, i + 1, length(str) - i - 1), width);
    i = i - 1;
  }

  return [section(str, 0, i) + "\n"] + wrap-string(section(str, i, length(str) - i), width);
}

define function string-wrap-string(str, width) {
  reduce(function (a, l) a + l, "", wrap-string(str, width));
}

checkpoint();
shutdown();
.
