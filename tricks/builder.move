// room builder

define function build-rooms(n) {
  define orm = location(realuid());

  while (n > 0) {
    define name = "room " + get-print-string(n);
    define r = Room:clone();

    r:set-name(name);
    move-to(r, null);
    Exit:dig("f", orm, r);
    Exit:dig("b", r, orm);
    orm = r;

    n = n - 1;
  }
}

