// Crawler-body method for a spider implemented using spider.verbs.move
// This method registers all unregistered objects that it sees.

define method (TARGET) crawler-body(obj, numsteps) {
  if (slot-clear?(obj, #registry-number)) {
    Registry:register(obj);
    realuid():tell("Registered " + obj:fullname() + "\n");
  }
}
