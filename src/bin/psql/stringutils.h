/*
 * psql - the PostgreSQL interactive terminal
 *
 * Copyright (c) 2000-2013, PostgreSQL Global Development Group
 *
 * src/bin/psql/stringutils.h
 */
#ifndef STRINGUTILS_H
#define STRINGUTILS_H

/* The cooler version of strtok() which knows about quotes and doesn't
 * overwrite your input */
extern char *strtokx(const char *s,
		const char *whitespace,
		const char *delim,
		const char *quote,
		char escape,
		bool e_strings,
		bool del_quotes,
		int encoding);

extern void strip_quotes(char *source, char quote, char escape, int encoding);

extern char *quote_if_needed(const char *source, const char *entails_quote,
				char quote, char escape, int encoding);


/* Returns lenght of string after escaping for COPY TEXT*/
extern int get_escaped_for_copy_len(const char* c);

/* Copies c to dst, escaping according to COPY TEXT format. Retuns pointer past the last character copied.
 * Assumes there is enough room in the dst buffer (use get_escaped_for_copy_len to calculate the size beforehand)
 */
extern char* escape_for_copy(char* dst, const char* c);

#endif   /* STRINGUTILS_H */
