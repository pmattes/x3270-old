/*      
 * Copyright 2000 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 */     

/*     
 *	printerc.h
 *		Printer session support
 */             

extern void printer_lu_dialog(void);
extern void printer_start(const char *lu);
extern void printer_stop(void);
extern Boolean printer_running(void);
