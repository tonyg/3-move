// Spider that registers all accessible rooms and their contents.

{
  define ptell = realuid():tell;

  define function regspider(o, stk) {
    define function already-met?(x, stk) {
      if (stk == null)
	return false;
      if (x == stk[0])
	return true;
      return already-met?(x, stk[1]);
    }

    if (slot-clear?(o, #registry-number)) {
      Registry:register(o);
      ptell("Registered " + o:fullname() + "\n");
    }

    stk = [o, stk];
    define function f(x) if (!already-met?(x, stk)) regspider(x, stk);

    for-each(f, contents(o));
    if (isa(o, Room))
      for-each(function (x) f(x.dest), o.exits);
  }

  fork/quota(function () {
    regspider(location(realuid()), null);
    ptell("Done.\n");
  }, -2);
}
