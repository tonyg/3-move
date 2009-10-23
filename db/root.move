// 3-MOVE, a multi-user networked online text-based programmable virtual environment
// Copyright 1997, 1998, 1999, 2003, 2005, 2008, 2009 Tony Garnock-Jones <tonyg@kcbbs.gen.nz>
//
// 3-MOVE is free software: you can redistribute it and/or modify it
// under the terms of the GNU Affero General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// 3-MOVE is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public
// License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with 3-MOVE.  If not, see <http://www.gnu.org/licenses/>.
//
// root.move

strip-object-clean(Root);

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

define function make-method-overridable(meth, val) {
  define fl = method-flags(meth);
  if (val)
    fl = fl | O_C_FLAG;
  else
    fl = fl & ~O_C_FLAG;
  set-method-flags(meth, fl);
}

{
  if (real-clone == undefined) {
    define the-clone = clone;
    clone = null;
    define-global(#real-clone, function () {
      define caller = caller-effuid();
      if (caller == null || privileged?(caller))
	the-clone;
      else
	#no-permission-real-clone;
    });
  }
}

{
  define the-clone = real-clone();

  define method (Root) clone() {
    define n = the-clone(this);
    if (n) {
      set-object-flags(n, O_NORMAL_FLAGS);
      n:initialize();
    }
    n;
  }
  set-setuid(Root:clone, false);
}

define method (Root) initialize() true;
make-method-overridable(Root:initialize, true);

define function clone-if-undefined(name, parent) {
  define val = symbol-value(name);

  if (val == undefined)
    define-global(name, val = parent:clone());
  else {
    strip-object-clean(val);
    val:initialize();
  }

  val;
}

if (Wizard == undefined)
  define-global(#Wizard, Root:clone());
else
  strip-object-methods(Wizard);
set-privileged(Wizard, true);
set-owner(Wizard, Wizard);
set-owner(Root, Wizard);
set-realuid(Wizard);
set-effuid(Wizard);
set-object-flags(Root, O_NORMAL_FLAGS);
set-object-flags(Wizard, O_NORMAL_FLAGS);

define (Root) name = "The Root Object";
set-slot-flags(Root, #name, O_ALL_R);

define method (Root) set-name(n) {
  define c = caller-effuid();

  if (c == owner(this) || privileged?(c))
    this.name = n;
  else
    false;
}

define method (Root) fullname() {
  if (slot-clear?(this, #registry-number))
    this.name;
  else
    this.name + " (#" + get-print-string(this.registry-number) + ")";
}

Wizard:set-name("Wizard");

move-to(Root, null);

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

define (Root) Genders = make-hashtable(23);
set-slot-flags(Root, #Genders, O_OWNER_MASK);

Root.Genders[#neuter] = ["it", "it", "its", "it's", "itself"];
Root.Genders[#male] = ["he", "him", "his", "his", "himself"];
Root.Genders[#female] = ["she", "her", "her", "hers", "herself"];

checkpoint();
shutdown();
.
