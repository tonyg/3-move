#ifndef Parser_H
#define Parser_H

typedef struct ParseInst {
  VMSTATE vms;
  SCANINST scaninst;
  int need_scan;
  int scan_result;
} ParseInst, *PARSEINST;

extern OVECTOR parse(VMSTATE vms, SCANINST scaninst);

#endif
