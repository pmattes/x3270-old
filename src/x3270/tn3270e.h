/*
 * Copyright 1996, 1999 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 */

/*
 *	tn3270e.h
 *
 *		Header file for the TN3270E Protocol, RFC 1647.
 */

/* TELNET option. */
#if !defined(TELOPT_TN3270E) /*[*/
#define TELOPT_TN3270E			40
#endif /*]*/

/* Operations. */
#define TN3270E_OP_ASSOCIATE		0
#define TN3270E_OP_CONNECT		1
#define TN3270E_OP_DEVICE_TYPE		2
#define TN3270E_OP_FUNCTIONS		3
#define TN3270E_OP_IS			4
#define TN3270E_OP_REASON		5
#define TN3270E_OP_REJECT		6
#define TN3270E_OP_REQUEST		7
#define TN3270E_OP_SEND			8

/* Reason-codes. */
#define TN3270E_REASON_CONN_PARTNER	0
#define TN3270E_REASON_DEVICE_IN_USE	1
#define TN3270E_REASON_INV_ASSOCIATE	2
#define TN3270E_REASON_INV_DEVICE_NAME	3
#define TN3270E_REASON_INV_DEVICE_TYPE	4
#define TN3270E_REASON_TYPE_NAME_ERROR	5
#define TN3270E_REASON_UNKNOWN_ERROR	6
#define TN3270E_REASON_UNSUPPORTED_REQ	7

/* Function Names. */
#define TN3270E_FUNC_BIND_IMAGE		0
#define TN3270E_FUNC_DATA_STREAM_CTL	1
#define TN3270E_FUNC_RESPONSES		2
#define TN3270E_FUNC_SCS_CTL_CODES	3
#define TN3270E_FUNC_SYSREQ		4
