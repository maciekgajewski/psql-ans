/*
 * psql - the PostgreSQL interactive terminal
 *
 * Copyright (c) 2013, PostgreSQL Global Development Group
 *
 * src/bin/psql/asn.h
 */
#ifndef ASN_H
#define ASN_H

#include "libpq-fe.h"


/* Cache of last N query results, stored in a format ready to insert itno tempary table when needed */
struct _asn 
{
	int		numColumns;
	Oid		*columnTypes;
	char	**columnNames;
	char	*data;
	
	char	*name; // hiostory item name
	char	*tableName; // corresponding table in DB. NULL if not created yet
	
	struct _asn	*next;
};

typedef struct _asn* AsnHistory;

AsnHistory	CreateAsnHistory(void);
void		AddToHistory(AsnHistory history, PGresult* result);


/*
 * Takes variable name and checks wether it matches the name of existing history item.
 * If none found, returns NULL.
 * If found, but table not created yet, creates the table first.
 */
const char* GetOrCreateTable(AsnHistory history, PGconn *db, const char* name);

#endif
